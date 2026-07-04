#ifndef NETWORK_H
#define NETWORK_H

#include "Globals.h"
#include <AsyncTCP.h>
#include <Arduino_JSON.h>
#include <LittleFS.h>
#include <ESPmDNS.h>

static JSONVar networkOptions;
static bool wifiJoinInProgress = false;
static bool wifiJoinSuccess = false;
static String wifiJoinMessage = "Idle";
static String wifiJoinSsid = "";
static String wifiJoinPassword = "";
static bool wifiJoinFromCaptivePortal = false;
static uint32_t wifiJoinStartMs = 0;
static uint32_t wifiJoinLastDotMs = 0;
static const uint32_t WIFI_JOIN_TIMEOUT_MS = 50000;
static bool captiveDnsRunning = false;

String getHaConfigJson();
bool testHaConnection(const String &serverName, const String &token, String &errorText);
bool fetchHaLights(const String &serverName, const String &token, String &lightsJson, String &errorText);
void clearWandMappings();
void saveWandMapping(uint8_t slot, uint32_t wandId, const String &action, const String &entityId);
inline void setupAP();
inline void StartCaptivePortal();
inline void WiFiStationSetup(String rec_ssid, String rec_password);
inline bool startWiFiJoinTask(const String &rec_ssid, const String &rec_password, String &errorText);
inline void processWiFiJoinTask();
inline String getWiFiJoinStatusJson();
inline void NetworkTick();
inline void shutdownCaptiveApIfActive();

inline String postParam(AsyncWebServerRequest *request, const char *name) {
	if (request->hasParam(name, true)) {
		return request->getParam(name, true)->value();
	}
	return "";
}

inline bool isCaptiveMode() {
	return (!is_setup_done || WiFi.getMode() == WIFI_MODE_AP);
}

inline bool ensureConfigAuth(AsyncWebServerRequest *request) {
	if (isCaptiveMode()) {
		return true;
	}
	String user = "admin";
	String pass = "admin123";
	if (preferences.isKey("cfg_user")) {
		user = preferences.getString("cfg_user", "admin");
	}
	if (preferences.isKey("cfg_pass")) {
		pass = preferences.getString("cfg_pass", "admin123");
	}
	if (!request->authenticate(user.c_str(), pass.c_str())) {
		request->requestAuthentication();
		return false;
	}
	return true;
}

inline void redirectToCaptivePortal(AsyncWebServerRequest *request) {
	String captiveUrl = String("http://") + WiFi.softAPIP().toString() + "/";
	request->redirect(captiveUrl);
}

inline String getDeviceStatusJson() {
	JSONVar status;
	status["is_setup_done"] = is_setup_done;
	status["ssid"] = preferences.getString("rec_ssid", "");
	status["ip"] = WiFi.localIP().toString();
	status["hostname"] = preferences.getString("hostname", hostname);
	status["hostname_url"] = String("http://") + preferences.getString("hostname", hostname) + ".local";
	status["mac"] = WiFi.macAddress();
	return JSON.stringify(status);
}

inline String getLastSeenWandJson() {
	JSONVar status;
	status["success"] = true;
	status["seen"] = hasLastDetectedWand;
	if (hasLastDetectedWand) {
		status["wand_id"] = (double)lastDetectedWandId;
		status["magnitude"] = lastDetectedMagnitude;
		status["age_ms"] = (double)(millis() - lastDetectedAtMs);
	}
	return JSON.stringify(status);
}

inline String getWiFiJoinStatusJson() {
	JSONVar status;
	status["in_progress"] = wifiJoinInProgress;
	status["success"] = wifiJoinSuccess;
	status["message"] = wifiJoinMessage;
	status["ip"] = WiFi.localIP().toString();
	status["hostname"] = preferences.getString("hostname", hostname);
	status["hostname_url"] = String("http://") + preferences.getString("hostname", hostname) + ".local";
	return JSON.stringify(status);
}

inline bool startWiFiJoinTask(const String &rec_ssid, const String &rec_password, String &errorText) {
	if (wifiJoinInProgress) {
		errorText = "WiFi connection is already in progress";
		return false;
	}
	if (rec_ssid.length() == 0) {
		errorText = "SSID is required";
		return false;
	}

	wifiJoinInProgress = true;
	wifiJoinSuccess = false;
	wifiJoinMessage = "Connecting to WiFi...";
	wifiJoinSsid = rec_ssid;
	wifiJoinPassword = rec_password;
	wifiJoinFromCaptivePortal = !is_setup_done;
	wifiJoinStartMs = millis();
	wifiJoinLastDotMs = 0;

	if (wifiJoinFromCaptivePortal) {
		WiFi.mode(WIFI_AP_STA);
	} else {
		WiFi.mode(WIFI_STA);
	}

	WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
	WiFi.setHostname(hostname.c_str());
	WiFi.begin(wifiJoinSsid.c_str(), wifiJoinPassword.c_str());
	Serial.print("[WIFI] Join started for SSID: ");
	Serial.println(wifiJoinSsid);
	return true;
}

inline void processWiFiJoinTask() {
	if (!wifiJoinInProgress) {
		return;
	}

	if (WiFi.status() == WL_CONNECTED) {
		wifiJoinInProgress = false;
		wifiJoinSuccess = true;
		wifiJoinMessage = "WiFi connected";
		is_setup_done = true;
		preferences.putBool("is_setup_done", is_setup_done);
		preferences.putString("rec_ssid", wifiJoinSsid);
		preferences.putString("rec_password", wifiJoinPassword);
		Serial.println();
		Serial.print("WiFi connected to: ");
		Serial.println(wifiJoinSsid);
		Serial.print("IP address: ");
		Serial.println(WiFi.localIP());
		if (MDNS.begin(preferences.getString("hostname", hostname).c_str())) {
			Serial.println("mDNS responder started");
		}
		blink_LED((char *)"FAST", 5000);
		return;
	}

	if (millis() - wifiJoinLastDotMs > 1000) {
		Serial.print(".");
		blink_LED((char *)"SLOW", 120);
		wifiJoinLastDotMs = millis();
	}

	if (millis() - wifiJoinStartMs > WIFI_JOIN_TIMEOUT_MS) {
		wifiJoinInProgress = false;
		wifiJoinSuccess = false;
		wifiJoinMessage = "Unable to connect. Check SSID/password and retry.";
		is_setup_done = false;
		preferences.putBool("is_setup_done", is_setup_done);
		Serial.println();
		Serial.println("Timeout connecting to WiFi. The SSID and Password seem incorrect.");
	}
}

inline void NetworkTick() {
	if (captiveDnsRunning) {
		dnsServer.processNextRequest();
	}
	processWiFiJoinTask();
}

inline void shutdownCaptiveApIfActive() {
	if (WiFi.getMode() == WIFI_MODE_APSTA || WiFi.getMode() == WIFI_MODE_AP) {
		if (captiveDnsRunning) {
			dnsServer.stop();
			captiveDnsRunning = false;
		}
		WiFi.softAPdisconnect(true);
		WiFi.mode(WIFI_STA);
		Serial.println("[WIFI] Captive AP stopped; device is now STA-only");
	}
}

inline void setupServer() {
	// Captive portal probe endpoints used by Android/iOS/Windows.
	server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (isCaptiveMode()) {
			redirectToCaptivePortal(request);
			return;
		}
		request->send(204);
	});

	server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (isCaptiveMode()) {
			redirectToCaptivePortal(request);
			return;
		}
		request->send(204);
	});

	server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (isCaptiveMode()) {
			redirectToCaptivePortal(request);
			return;
		}
		request->send(200, "text/plain", "Success");
	});

	server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (isCaptiveMode()) {
			redirectToCaptivePortal(request);
			return;
		}
		request->send(200, "text/plain", "Microsoft NCSI");
	});

	server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (isCaptiveMode()) {
			redirectToCaptivePortal(request);
			return;
		}
		request->send(200, "text/plain", "Microsoft Connect Test");
	});

	server.on("/redirect", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (isCaptiveMode()) {
			redirectToCaptivePortal(request);
			return;
		}
		request->send(200, "text/plain", "OK");
	});

	server.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (isCaptiveMode()) {
			redirectToCaptivePortal(request);
			return;
		}
		request->send(200, "text/plain", "OK");
	});

	server.on("/jquery-3.7.1.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(LittleFS, "/jquery-3.7.1.min.js", "text/javascript", false);
	});

	server.on("/availablenetworks", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "application/json", JSON.stringify(networkOptions));
	});

	server.on("/getconfigs", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (!ensureConfigAuth(request)) {
			return;
		}
		request->send(200, "application/json", getHaConfigJson());
	});

	server.on("/device/status", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (!ensureConfigAuth(request)) {
			return;
		}
		request->send(200, "application/json", getDeviceStatusJson());
	});

	server.on("/wand/last", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (!ensureConfigAuth(request)) {
			return;
		}
		request->send(200, "application/json", getLastSeenWandJson());
	});

	server.on("/wifi/status", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "application/json", getWiFiJoinStatusJson());
	});

	server.on("/wifi/finalize", HTTP_POST, [](AsyncWebServerRequest *request) {
		JSONVar response;
		if (!is_setup_done || WiFi.status() != WL_CONNECTED) {
			response["success"] = false;
			response["message"] = "WiFi is not connected yet";
			request->send(400, "application/json", JSON.stringify(response));
			return;
		}

		shutdownCaptiveApIfActive();
		response["success"] = true;
		response["message"] = "AP shut down";
		response["ip"] = WiFi.localIP().toString();
		response["hostname"] = preferences.getString("hostname", hostname);
		response["hostname_url"] = String("http://") + preferences.getString("hostname", hostname) + ".local";
		request->send(200, "application/json", JSON.stringify(response));
	});

	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (isCaptiveMode()) {
			request->send(LittleFS, "/sta-index.html", "text/html", false);
		} else {
			if (!ensureConfigAuth(request)) {
				return;
			}
		  request->send(LittleFS, "/index.html", "text/html", false);
		}
	});

	server.on("/wifi/connect", HTTP_POST, [](AsyncWebServerRequest *request) {
		ssid = postParam(request, "wifissid");
		password = postParam(request, "wifipass");
		String errorText;
		JSONVar response;

		if (!startWiFiJoinTask(ssid, password, errorText)) {
			response["success"] = false;
			response["in_progress"] = wifiJoinInProgress;
			response["message"] = errorText;
			request->send(400, "application/json", JSON.stringify(response));
		} else {
			response["success"] = false;
			response["in_progress"] = true;
			response["message"] = "Connecting to WiFi...";
			request->send(200, "application/json", JSON.stringify(response));
		}
	});

	server.on("/ha/test", HTTP_POST, [](AsyncWebServerRequest *request) {
		if (!ensureConfigAuth(request)) {
			return;
		}
		String inputToken = postParam(request, "token");
		String inputUrl = postParam(request, "haurl");
		Serial.println("[HA] /ha/test request received");

		JSONVar response;
		String errorText;

		if (inputToken.length() == 0 || inputUrl.length() == 0) {
			response["success"] = false;
			response["message"] = "Both token and Home Assistant URL are required";
			request->send(400, "application/json", JSON.stringify(response));
			return;
		}

		bool ok = testHaConnection(inputUrl, inputToken, errorText);
		response["success"] = ok;
		response["message"] = ok ? "Connection successful" : errorText;
		Serial.print("[HA] /ha/test result: ");
		Serial.println(ok ? "success" : "failed");
		request->send(200, "application/json", JSON.stringify(response));
	});

	server.on("/ha/lights", HTTP_POST, [](AsyncWebServerRequest *request) {
		if (!ensureConfigAuth(request)) {
			return;
		}
		Serial.println("[HA] /ha/lights request received");
		String token = postParam(request, "token");
		String url = postParam(request, "haurl");
		if (token.length() == 0) {
			token = preferences.getString("token", "");
		}
		if (url.length() == 0) {
			url = preferences.getString("haurl", "");
		}

		String lightsJson;
		String errorText;

		if (token.length() == 0 || url.length() == 0) {
			request->send(400, "application/json", "{\"success\":false,\"message\":\"Token and Home Assistant URL are required\"}");
			return;
		}

		if (fetchHaLights(url, token, lightsJson, errorText)) {
			String payload = String("{\"success\":true,\"lights\":") + lightsJson + "}";
			Serial.println("[HA] /ha/lights result: success");
			request->send(200, "application/json", payload);
		} else {
			Serial.print("[HA] /ha/lights result: failed - ");
			Serial.println(errorText);
			errorText.replace("\\", "\\\\");
			errorText.replace("\"", "\\\"");
			String payload = String("{\"success\":false,\"message\":\"") + errorText + "\"}";
			request->send(200, "application/json", payload);
		}
	});

	server.on("/savesettings", HTTP_POST, [](AsyncWebServerRequest *request) {
		if (!ensureConfigAuth(request)) {
			return;
		}
		String token = normalizeHaToken(postParam(request, "token"));
		String haurl = normalizeHaUrl(postParam(request, "haurl"));
		String host = postParam(request, "hostname");
		bool haValidatedForSubmittedValues = postParam(request, "ha_validated") == "1";
		String previousToken = normalizeHaToken(preferences.getString("token", ""));
		String previousHaUrl = normalizeHaUrl(preferences.getString("haurl", ""));
		bool haCredentialsChanged = !areHaCredentialsEqual(haurl, token, previousHaUrl, previousToken);
		bool haValidated = isHaConfigValidated();

		preferences.putString("token", token);
		preferences.putString("haurl", haurl);
		if (token.length() == 0 || haurl.length() == 0) {
			haValidated = false;
		} else if (haCredentialsChanged) {
			haValidated = haValidatedForSubmittedValues;
		} else if (haValidatedForSubmittedValues) {
			haValidated = true;
		}
		setHaConfigValidated(haValidated);
		if (host.length() > 0) {
			preferences.putString("hostname", host);
			hostname = host;
			MDNS.end();
			MDNS.begin(hostname.c_str());
		}

		clearWandMappings();
		for (uint8_t i = 0; i < 10; i++) {
			String wand = postParam(request, (String("wand_id_") + i).c_str());
			String action = postParam(request, (String("wand_action_") + i).c_str());
			String entity = postParam(request, (String("wand_entity_") + i).c_str());
			uint32_t wandId = (uint32_t)wand.toInt();
			if (wandId != 0 && action.length() > 0 && entity.length() > 0) {
				saveWandMapping(i, wandId, action, entity);
			}
		}

		if (is_setup_done) {
			shutdownCaptiveApIfActive();
		}

		request->send(200, "application/json", "{\"success\":true,\"message\":\"Settings saved\"}");
	});
}

inline void refreshWiFiList() {
	int numWiFi = WiFi.scanNetworks();
	delay(200);
	for (int i = 0; i < numWiFi; i++) {
		networkOptions[i] = WiFi.SSID(i);
	}
}


class CaptiveRequestHandler : public AsyncWebHandler {
public:
    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}
    bool canHandle(AsyncWebServerRequest *request) { return true; }
    void handleRequest(AsyncWebServerRequest *request) {
		if (isCaptiveMode()) {
			redirectToCaptivePortal(request);
			return;
		}
		request->send(404, "text/plain", "Not found");
	}
};

inline void setupAP() {
    String APName = preferences.getString("hostname", "esp32C3-1");
    Serial.println();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(APName);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
}

inline void StartCaptivePortal() {
    Serial.println("Setting up AP Mode");
    setupAP();
    Serial.println("Setting up Async WebServer");
    setupServer();
    Serial.println("Starting DNS Server");
    dnsServer.start(53, "*", WiFi.softAPIP());
	captiveDnsRunning = true;
    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
    server.begin();
    dnsServer.processNextRequest();
    refreshWiFiList();
}

inline void WiFiStationSetup(String rec_ssid, String rec_password) {
	bool wifiTimeout = false;
	bool joiningFromCaptivePortal = !is_setup_done;
	if (joiningFromCaptivePortal) {
		WiFi.mode(WIFI_AP_STA);
	} else {
		WiFi.mode(WIFI_STA);
	}
	char ssid_arr[64];
	char password_arr[64];
	rec_ssid.toCharArray(ssid_arr, rec_ssid.length() + 1);
	rec_password.toCharArray(password_arr, rec_password.length() + 1);
	WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
	WiFi.setHostname(hostname.c_str());
	WiFi.begin(ssid_arr, password_arr);

	uint32_t t1 = millis();
  uint32_t lastDotMs = 0;
	while (WiFi.status() != WL_CONNECTED) {
		delay(250);
    if (millis() - lastDotMs > 1000) {
		  Serial.print(".");
      blink_LED((char *)"SLOW", 120);
      lastDotMs = millis();
    }
		if (millis() - t1 > 50000) {
			Serial.println();
			Serial.println("Timeout connecting to WiFi. The SSID and Password seem incorrect.");
			is_setup_done = false;
			preferences.putBool("is_setup_done", is_setup_done);
			StartCaptivePortal();
			wifiTimeout = true;
			break;
		}
	}
	if (!wifiTimeout) {
		is_setup_done = true;
		Serial.println("");
		Serial.print("WiFi connected to: ");
		Serial.println(rec_ssid);
		Serial.print("IP address: ");
		Serial.println(WiFi.localIP());
		preferences.putBool("is_setup_done", is_setup_done);
		preferences.putString("rec_ssid", rec_ssid);
		preferences.putString("rec_password", rec_password);
		if (MDNS.begin(preferences.getString("hostname", hostname).c_str())) {
      Serial.println("mDNS responder started");
    }
		blink_LED((char *)"FAST", 5000);
	}
}

#endif // NETWORK_H
