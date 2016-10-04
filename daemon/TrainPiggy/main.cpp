#include <cstdlib>
#include <stdio.h>


#include <iostream>
#include <sstream>
using namespace std;


#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
using namespace curlpp::options;

#include "json.hpp"
using json = nlohmann::json;

// sleeping using C++11 features
#include <chrono>
#include <thread>

#include "gpio.cpp"

void logCancellation(json item)
{
    string time;
    
    if (!item["eta"].empty())
    {
        time = item["sta"];
    }
    else
    {
        time = item["std"];
    }

    cout << "Cancellation - operator: " << item["operator"] << ", time: " << time << ", from: " << item["origin"]["location"]["crs"] << ", to: " << item["destination"]["location"]["crs"] << endl;
}

bool processService(json item)
{
    bool canellation;
    auto eta = item["eta"];
    auto etd = item["etd"];
    
    if (!eta.empty() && eta == "Cancelled")
    {
        logCancellation(item);
        canellation = true;
    }
    else if (!etd.empty() && etd == "Cancelled")
    {
        logCancellation(item);
        canellation = true;
    }
    
    return canellation;
}

bool processServices(json services)
{
    bool cancellations;

    json service;
    for (json::iterator it = services.begin(); !cancellations && it != services.end(); ++it)
    {
        service = it.value();
        cancellations = processService(service);
    }
    
    return cancellations;
}

bool processDocument(json root)
{
    json departures = root["departures"]["trainServices"]["service"];
    json arrivals = root["arrivals"]["trainServices"]["service"];
    
    cout << "Checking station " << root["departures"]["locationName"] << "..." << endl;

    bool cancellations = processServices(departures) || processServices(arrivals);
    return cancellations;
}

bool checkStation(string stationCode)
{
    bool cancellations;

    try
    {
        cout << "Polling station '" << stationCode << "'..." << endl;
        
        // Make request to API
        curlpp::Cleanup myCleanup;

        curlpp::Easy myRequest;
        myRequest.setOpt(Url("https://www.abelliogreateranglia.co.uk/__live/board/" + stationCode));
        myRequest.perform();

        // Read request
        std::ostringstream os;
        os << myRequest;
        string response = os.str();

        // Parse as JSON
        json j = json::parse(response);

        // Process for cancellations
        cancellations = processDocument(j);
    }
    catch(curlpp::RuntimeError & e)
    {
        std::cout << e.what() << std::endl;
    }
    catch(curlpp::LogicError & e)
    {
        std::cout << e.what() << std::endl;
    }
    
    return cancellations;
}

bool checkStations()
{
    bool cancellations;

    cancellations |= checkStation("NRW");
    cancellations |= checkStation("CBG");

    return cancellations;
}

void setEyeLights(bool cancellations)
{
#ifdef MOCK
    cout << "Mock set lights to: " << cancellations << endl;
#else
    GPIOWrite(POUT, cancellations);
    cout << "Set lights to: " << cancellations << endl;
#endif
}

int setupEyeLights()
{
#ifdef MOCK
    cout << "Mock setup lights" << endl;
#else
    // Setup GPIO
    if (GPIOExport(POUT))
    {
        cout << "Failed to setup/export GPIO" << endl;
        return 1;
    }
    if (GPIODirection(POUT, OUT) == -1)
    {
        cout << "Failed to set direction of GPIO" << endl;
        return 2;
    }
#endif
    // Set eyes to off initially
    setEyeLights(false);
    
    return 0;
}

// Function for blinking lights, ran as separate thread
bool cancellations;
void lightsThreadMain()
{
    cout << "Started lights thread..." << endl;

    // Flash lights initially as test
    for (int i = 0; i < 5; i++)
    {
        this_thread::sleep_for(chrono::seconds(1));
        setEyeLights(true);
        this_thread::sleep_for(chrono::seconds(1));
        setEyeLights(false);
    }
    
    // Loop forever, watching shared var...
    bool blinkPreviousState = false;
    while (true)
    {
        if (cancellations)
        {
            // Invert previous state and use to toggle eye lights
            blinkPreviousState = !blinkPreviousState;
            setEyeLights(blinkPreviousState);
        }
        else if (blinkPreviousState != cancellations)
        {
            // Need to turn LEDs off...
            blinkPreviousState = false;
            setEyeLights(false);
        }
        
        this_thread::sleep_for(chrono::seconds(2));
    }
}

int main(int argc, char** argv)
{
    bool previousState;
    
    // Setup eyes
    int setup = setupEyeLights();
    if (setup != 0)
    {
        return setup;
    }
    
    // Startup lights thread
    thread threadLights(lightsThreadMain);

    // Forever poll the status of stations...
    while (true)
    {
        // Check for cancellations
        cancellations = checkStations();

        // Check if to change eyes...
        if (cancellations != previousState)
        {
            previousState = cancellations;
            setEyeLights(cancellations);
        }
        
        // Sleep...
        cout << "Done, sleeping..." << endl;
        this_thread::sleep_for(chrono::seconds(60));
    }
    
    return 0;
}
