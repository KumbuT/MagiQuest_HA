#include <WiFi.h>
#include <DNSServer.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <IRremote.hpp>
#include <HTTPClient.h>
#include "FS.h"
#include <LittleFS.h>
#include <Preferences.h>

//  You only need to format LittleFS the first time you run a
//  test or else use the LITTLEFS plugin to create a partition
//  https://github.com/lorol/arduino-esp32littlefs-plugin

#define FORMAT_LITTLEFS_IF_FAILED true


#define IR_RECV_PIN D2
#define LED_FEEDBACK_PIN D6

Preferences preferences;
String ssid;
String password;
bool valid_ssid_received = false;
bool valid_password_received = false;
bool is_setup_done = false;
bool wifi_timeout = false;
JSONVar networkOptions;
JSONVar haConfig;
const byte DNS_PORT = 53;
IPAddress apIP(8, 8, 4, 4);  // The default android DNS
DNSServer dnsServer;
AsyncWebServer server(80);
String hostname = "HA MagiQuest 1";



void blink_LED(char *mode = "SLOW", int duration = 5000) {
  int interval = 200;
  if (mode == "SLOW") {
    interval = 50;
  } else {
    interval = 200;
  }

  int t = millis();
  while (millis() - t < duration) {
    digitalWrite(LED_FEEDBACK_PIN, HIGH);
    delay(interval);
    digitalWrite(LED_FEEDBACK_PIN, LOW);
    delay(interval);
  }
}
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\r\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}


class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request) {
    //request->addInterestingHeader("ANY");
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    //request->send_P(200, "text/html", index_HTML);
    request->send(LittleFS, "/index.html", "text/html", false);
  }
};

void setupServer() {
  // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   //request->send_P(200, "text/html", index_HTML);
  //   request->send(LittleFS, "/index.html", "text/html", false);
  //   Serial.println("Client Connected");
  // });

  server.on("/jquery-3.7.1.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    //request->send_P(200, "text/html", index_HTML);
    request->send(LittleFS, "/jquery-3.7.1.min.js", "text/javascript", false);
    Serial.println("JS file requested");
  });

  server.on("/availablenetworks", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", JSON.stringify(networkOptions));
  });

  server.on("/getconfigs", HTTP_GET, [](AsyncWebServerRequest *request) {
    loadHaConfig();
    request->send(200, "application/json", JSON.stringify(haConfig));
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/sta-index.html", "text/html", false);
    Serial.println("STA Index Requested");
  });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("wifissid")) {
      ssid = request->getParam("wifissid")->value();
      valid_ssid_received = true;
    }

    if (request->hasParam("wifipass")) {
      password = request->getParam("wifipass")->value();
      valid_password_received = true;
    }
    request->send_P(200, "text/html", "The values entered by you have been successfully sent to the device. It will now attempt WiFi connection<br><a href=\"/\">Return to Home Page</a>");
  });


  server.on("/savesettings", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("token")) {
      preferences.putString("token", request->getParam("token")->value());
    }
    if (request->hasParam("haurl")) {
      preferences.putString("haurl", request->getParam("haurl")->value());
    }
    if (request->hasParam("hostname")) {
      preferences.putString("hostname", request->getParam("hostname")->value());
    }
    Serial.print("Token Received: ");
    Serial.println(request->getParam("token")->value());
    Serial.print("HA URL Received: ");
    Serial.println(request->getParam("haurl")->value());
    request->send(200, "text/plain", "Settings saved!");
  });
}

void refreshWiFiList() {
  int numWiFi = WiFi.scanNetworks();
  delay(200);
  for (int i = 0; i < numWiFi; i++) {
    networkOptions[i] = WiFi.SSID(i);
  }
}

void StartCaptivePortal() {
  Serial.println("Setting up AP Mode");
  setupAP();
  Serial.println("Setting up Async WebServer");
  setupServer();
  Serial.println("Starting DNS Server");
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);  //only when requested from AP
  server.begin();
  dnsServer.processNextRequest();
  refreshWiFiList();
}

void setupAP() {
  String APName = preferences.getString("hostname", "esp32C3-1");
  //setup Access Point for Wifi Credentials
  Serial.println();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(APName);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
}

void WiFiStationSetup(String rec_ssid, String rec_password) {
  wifi_timeout = false;
  WiFi.mode(WIFI_STA);
  char ssid_arr[20];
  char password_arr[20];
  rec_ssid.toCharArray(ssid_arr, rec_ssid.length() + 1);
  rec_password.toCharArray(password_arr, rec_password.length() + 1);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(hostname.c_str());  //define hostname
  // Serial.print("Received SSID: ");
  // Serial.println(ssid_arr);
  // Serial.print("And password: ");
  // Serial.println(password_arr);
  WiFi.begin(ssid_arr, password_arr);

  uint32_t t1 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    Serial.print(".");
    blink_LED("SLOW", 2000);
    if (millis() - t1 > 50000)  //50 seconds elapsed connecting to WiFi
    {
      Serial.println();
      Serial.println("Timeout connecting to WiFi. The SSID and Password seem incorrect.");
      valid_ssid_received = false;
      valid_password_received = false;
      is_setup_done = false;
      preferences.putBool("is_setup_done", is_setup_done);
      StartCaptivePortal();
      wifi_timeout = true;
      break;
    }
  }
  if (!wifi_timeout) {
    is_setup_done = true;
    Serial.println("");
    Serial.print("WiFi connected to: ");
    Serial.println(rec_ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    preferences.putBool("is_setup_done", is_setup_done);
    preferences.putString("rec_ssid", rec_ssid);
    preferences.putString("rec_password", rec_password);
    blink_LED("FAST", 5000);
  }
}

void callHA() {
  const String serverName = preferences.getString("haurl", "1234");
  const String token = preferences.getString("token", "abcd");
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // HA IP address with path
    String apiURL = serverName;
    JSONVar myObject;
    // Assign the right entity_id based on WandID
    int wandID = IrReceiver.decodedIRData.address;
    // Serial.println("WandID= ");
    // Serial.print(wandID);
    switch (wandID) {
      case 64897:  //Advaya's MagiQuest WandID
        apiURL.concat("/api/states/light.advaya_s_light");
        myObject["entity_id"] = "light.advaya_s_light";
        break;
      case 51329:  //Kavya's MagiQuest WandID
        apiURL.concat("/api/states/light.kavya_s_light");
        myObject["entity_id"] = "light.kavya_s_light";
        break;
      default:  //Do nothing
        // apiURL.concat("/api/states/light.advaya_s_light");
        // myObject["entity_id"] = "light.advaya_s_light";
        return;
        break;
    };
    Serial.print("----GET-----");
    Serial.println(apiURL.c_str());
    http.begin(apiURL.c_str());
    //Send GET request
    //http.addHeader("Authorization", "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJmNTUwYzQzN2MyYmI0NjRjYmNkY2ZiYzhmODVhMmFhZCIsImlhdCI6MTcxMzc0Njg3MiwiZXhwIjoyMDI5MTA2ODcyfQ.NfF7Cfn2JEi11qfHiDrD59xR-emRWiTushfnS8RUGns");
    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      // Reset apiURL for next call
      apiURL = serverName;
      JSONVar response = JSON.parse(payload);
      String state = response["state"];
      if (state == "off") {
        apiURL.concat("/api/services/light/turn_on");
      } else {
        apiURL.concat("/api/services/light/turn_off");
      }
      Serial.println(payload);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      // Free resources before exiting
      http.end();
      return;
    }


    // Free resources
    http.end();

    Serial.print("----POST-----");
    Serial.println(apiURL.c_str());
    // Domain name with URL path or IP address with path
    http.begin(apiURL.c_str());
    //http.addHeader("Authorization", "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJmNTUwYzQzN2MyYmI0NjRjYmNkY2ZiYzhmODVhMmFhZCIsImlhdCI6MTcxMzc0Njg3MiwiZXhwIjoyMDI5MTA2ODcyfQ.NfF7Cfn2JEi11qfHiDrD59xR-emRWiTushfnS8RUGns");
    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("Content-Type", "application/json");
    // Send HTTP POST request
    httpResponseCode = http.POST(JSON.stringify(myObject));
    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println(payload);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    // Free resources
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

void printIrReceiver() {
  Serial.println("-----------------------------RECV START-----------------------------");
  //IrReceiver.printIRResultShort(&Serial);
  //IrReceiver.printIRSendUsage(&Serial);
  Serial.print("Protocol = ");
  Serial.println(IrReceiver.decodedIRData.protocol);
  Serial.print("WandID = ");
  Serial.println(IrReceiver.decodedIRData.address);
  Serial.print("Magnitude = ");
  Serial.println(IrReceiver.decodedIRData.command, DEC);
  Serial.println("-----------------------------RECV END-----------------------------");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_FEEDBACK_PIN, OUTPUT);
  digitalWrite(LED_FEEDBACK_PIN, LOW);
  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LITTLEFS");
    return;
  }
  listDir(LittleFS, "/", 2);

  Serial.begin(115200);
  Serial.println();
  preferences.begin("my-pref", false);
  is_setup_done = preferences.getBool("is_setup_done", false);
  ssid = preferences.getString("rec_ssid", "Sample_SSID");
  password = preferences.getString("rec_password", "abcdefgh");
  if (!is_setup_done) {
    StartCaptivePortal();
  } else {
    Serial.println("Using saved SSID and Password to attempt WiFi Connection!");
    // Serial.print("Saved SSID is ");
    // Serial.println(ssid);
    // Serial.print("Saved Password is ");
    // Serial.println(password);
    WiFiStationSetup(ssid, password);
    setupServer();
    server.begin();
    //load or Save settings
    loadHaConfig();
  }

  while (!is_setup_done) {
    dnsServer.processNextRequest();
    delay(10);
    if (valid_ssid_received && valid_password_received) {
      Serial.println("Attempting WiFi Connection!");
      WiFiStationSetup(ssid, password);
    }
  }

  // Setup IRReceiver
  Serial.println("Starting IRReceiver");
  IrReceiver.begin(IR_RECV_PIN, false);  // Start the receiver
  Serial.println("All Done!");
}

void loadHaConfig() {
  haConfig["haurl"] = (String)preferences.getString("haurl", "");
  haConfig["token"] = (String)preferences.getString("token", "");
  haConfig["ssid"] = (String)preferences.getString("rec_ssid", "") + " (RSSI: " + WiFi.RSSI() + " )";
  haConfig["password"] = (String)preferences.getString("rec_password", "");
  haConfig["hostname"] = (String)preferences.getString("hostname", hostname);
  haConfig["MAC"] = (String)WiFi.macAddress();
}

void loop() {
  if (IrReceiver.decode()) {
    uint16_t swingMagnitude = IrReceiver.decodedIRData.command;
    printIrReceiver();
    if (swingMagnitude > 10) {
      callHA();
      blink_LED("SLOW");
      // Call HA API if it was intentional swing
    }
    delay(1000);
    IrReceiver.resume();
  }
}
