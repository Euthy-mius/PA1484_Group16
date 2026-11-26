# PA1484 – Software Development: Public Transport Information and Interaction on ESP32

## Introduction
The purpose of the project was to develop a public transport information system that allows users to plan their journeys, view departure schedules, and check whether buses, trains, or ferries are delayed, in order to simplify their daily lives.
By using an ESP32 touchscreen device, we were able to display public transport information in a departure table. PlatformIO was used to upload the code to the device. To access public transport information, we used different APIs provided by Trafiklab.se such as Realtime API and ReseRobotV2.1. 

## Getting started

This project will run regardless of operating system, as long as Visual Studio Code and the extension "Platform IO" is installed.

### Visual Studio Code

1. Visit https://code.visualstudio.com/download and download the latest version.
2. Run Visual Studio Code and follow the installation steps.

### PlatformIO

1. Click the extensions tab on the left side.
2. Search for "PlatformIO IDE" and install it.

## Building and running

1. Open the locally cloned repository with Visual Studio Code
2. Open up the file "project/project.ino"
3. Connect your ESP32 to your computer via a USB cable.
4. Build and upload the project to the device.

### Required Additions

1. Wifi SSID and password is required to build the project, add a "secrets.h" file into the project, and add the following lines:
   static const char* WIFI_SSID = "SSID";
   static const char* WIFI_PASSWORD = "PWD";
2. This is also where future API keys are defined that are used in the project.

### Operation on Microcontroller

1. The microcontroller is operated by a touch screen to navigate between screens, and touch to press buttons.
2. Make sure Wifi is connected in order to get data from the API's.

## Features

Lastly, write which of the user stories you did and didn't develop in this project,  in the form of a checklist. Like this:

- [x] US1.1: As a user, I want to see a starting screen displaying the project name, current program
version, group number, and members' names.
- [x] US1.2B: As a user, I want to see the departure screen showing departure data: departure time,
line, destination, and departure status for any destinations in Karlskrona using live data from
Trafiklab (https://www.trafiklab.se/sv/api/our-apis/trafiklab-realtime-apis/)
- [x] US2.1: As a user, I want to navigate between different screens by sliding a finger over the
touchscreen.
- [x] US3.1: As a user, I want to access a settings screen.
- [x] US3.2B: As a user, I want to select different stops from US1.2B.
- [ ] US3.2C: As a user, I want to search and select different stops for US1.2B
- [x] US3.3: As a user, I want to select a transport type (bus, train, ferry, and all) using a dropdown.
- [x]  US3.4: As a user, I want to save my current settings as the default and have them restored when
the device restarts.
- [x] US3.5: As a user, I want to reset my current settings to the stored default settings.
- [x] US4.1: As a user, I want the live departure data to update automatically in 60 seconds.
- [x] US4.2B: As a user, I want to manually refresh the departure data with a button in the settings
screen.
- [x] US4.3: As a user, I want to see the current Wi-Fi connection and live data status on the settings
screen
- [x] US5.1: As a user, I want to access a journey selection screen.
- [x] US5.2: As a user, I want to select a starting stop and a final destination stop.
- [ ] US5.2B: As a user, I want to search for and select a starting stop and a final destination stop for US5.3
- [ ] US5.3B: As a user, I want to see the top matching journey, based on arrival time to the final
destination, that shows the departure time for each intermediate stop where I must change
transportation.

  
  


