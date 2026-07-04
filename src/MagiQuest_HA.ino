#include "../include/IRAdapter.h"
#include "../include/HAClient.h"
#include "../include/Storage.h"
#include "../include/Network.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[BOOT] MagiQuest starting...");
  pinMode(LED_FEEDBACK_PIN, OUTPUT);
  digitalWrite(LED_FEEDBACK_PIN, LOW);

  Serial.println("[BOOT] Initializing storage...");
  initStorage();
  Serial.println("[BOOT] Storage initialized");

  if (!is_setup_done) {
    StartCaptivePortal();
  } else {
    Serial.println("Using saved SSID and Password to attempt WiFi Connection!");
    WiFiStationSetup(ssid, password);
    if (is_setup_done) {
      setupServer();
      server.begin();
      loadHaConfig();
    }
  }

  while (!is_setup_done) {
    NetworkTick();
    delay(10);
    static uint32_t lastStatusMs = 0;
    if (millis() - lastStatusMs > 5000) {
      lastStatusMs = millis();
      Serial.print("[SETUP] Waiting for WiFi setup at ");
      Serial.println(WiFi.softAPIP());
    }
  }

  Serial.println("Starting IRReceiver");
  IrInit();
  Serial.println("All Done!");
}



void loop() {
  NetworkTick();
  if (IrDecodeAvailable()) {
    uint16_t swingMagnitude = IrGetMagnitude();
    printIrReceiver();
    if (swingMagnitude > 10) {
      callHA();
      blink_LED("SLOW");
      // Call HA API if it was intentional swing
    }
    delay(1000);
    IrResume();
  }
}
