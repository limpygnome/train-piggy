# Setup
## Requirements
- make / g++ / gcc - 4.9 and above

## Dependencies
Install curl.h (cURL library):
````
sudo apt-get install libcurl4-openssl-dev
````

Install cURL C++ wrapper:
````
sudo apt-get install libcurlpp-dev
````

## IDE / Compiler
Add flags to g++ and linker:
````
-lcurlpp -lcurl -std=gnu++11 -lpthread
````

# Building, Installation and Distribution

````
make
````
