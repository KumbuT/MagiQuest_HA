MagiQuest_HA
Convert your MagiQuest wand into a home automation trigger.

Description
This project allows you to use a MagiQuest wand to trigger various home automation actions. It uses an ESP32 microcontroller to capture IR signals from the wand and communicate with a Home Automation system over WiFi.

Features
Captive portal for WiFi setup
HTTP GET and POST requests to Home Automation server
LED feedback for status indication
IR receiver for capturing wand signals
Installation
Hardware Requirements
ESP32 microcontroller
IR receiver module
LED for feedback
Software Requirements
Arduino IDE
ESP32 board package
Necessary libraries: WiFi, DNSServer, AsyncTCP, ESPAsyncWebServer, Arduino_JSON, IRremote, HTTPClient, FS, LittleFS, Preferences
Setup
Clone the repository:

bash
git clone https://github.com/KumbuT/MagiQuest_HA.git
cd MagiQuest_HA
Open MagiQuest_HA.ino in Arduino IDE.

Install the required libraries:

WiFi
DNSServer
AsyncTCP
ESPAsyncWebServer
Arduino_JSON
IRremote
HTTPClient
FS
LittleFS
Preferences
Connect the hardware components:

IR receiver to pin D2
LED to pin D6
Upload the code to the ESP32.

Usage
Power on the ESP32. It will start in Access Point mode if WiFi credentials are not saved.
Connect to the ESP32's WiFi network and open a web browser to configure the WiFi settings.
Once connected to the WiFi, the ESP32 will attempt to connect to the Home Automation system.
Use the MagiQuest wand to trigger actions. The ESP32 will send HTTP requests to the configured Home Automation server.
Contributing
Contributions are welcome! Please open an issue or submit a pull request.

License
This project is licensed under the GNU General Public License v3.0. See the LICENSE file for details.

Contact
For any questions or suggestions, feel free to open an issue or contact the repository owner.
