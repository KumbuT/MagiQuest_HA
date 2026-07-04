#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <IPAddress.h>

#define IR_RECV_PIN D2
#define LED_FEEDBACK_PIN D6
#define FORMAT_LITTLEFS_IF_FAILED true

// Shared globals used by header-only components. These are `static` because
// the project is structured to include these headers only from the single
// translation unit (`src/MagiQuest_HA.ino`). If you later include these
// headers from multiple TUs, convert these to `extern` declarations and
// provide a single definitions file.

static Preferences preferences;
static String ssid;
static String password;
static bool is_setup_done = false;

static const byte DNS_PORT = 53;
static IPAddress apIP(8, 8, 4, 4);
static DNSServer dnsServer;
static AsyncWebServer server(80);
static String hostname = "HA MagiQuest 1";
static bool hasLastDetectedWand = false;
static uint32_t lastDetectedWandId = 0;
static uint16_t lastDetectedMagnitude = 0;
static uint32_t lastDetectedAtMs = 0;

#endif // GLOBALS_H
