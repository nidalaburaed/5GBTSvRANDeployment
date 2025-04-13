# Description

A script to configure and deploy 5G BTS (vRAN) on-Air 

# Requirements

C++ libraries for SSH, HTTP, and Selenium functionality

# How to run

Compile the script using the following command:

```bash
g++ -o main src/5GBTSvRANDeployment.cpp -lssh -lcurl -lwebdriverxx -std=c++17
```

Run the script by using the following command:

```bash
./5GBTSvRANDeployment
```