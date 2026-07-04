#ifndef HA_CLIENT_H
#define HA_CLIENT_H

#include "Globals.h"
#include "IRAdapter.h"

#include <HTTPClient.h>
#include <Arduino_JSON.h>

static JSONVar haConfig;
static const uint8_t MAX_WAND_MAPPINGS = 10;

inline String wandIdKey(uint8_t slot) {
	return String("wand_id_") + slot;
}

inline String actionKey(uint8_t slot) {
	return String("wand_action_") + slot;
}

inline String entityKey(uint8_t slot) {
	return String("wand_entity_") + slot;
}

inline void clearWandMappings() {
	for (uint8_t i = 0; i < MAX_WAND_MAPPINGS; i++) {
		preferences.putUInt(wandIdKey(i).c_str(), 0);
		preferences.putString(actionKey(i).c_str(), "");
		preferences.putString(entityKey(i).c_str(), "");
	}
}

inline void saveWandMapping(uint8_t slot, uint32_t wandId, const String &action, const String &entityId) {
	if (slot >= MAX_WAND_MAPPINGS) {
		return;
	}
	preferences.putUInt(wandIdKey(slot).c_str(), wandId);
	preferences.putString(actionKey(slot).c_str(), action);
	preferences.putString(entityKey(slot).c_str(), entityId);
}

inline bool getActionForWand(uint32_t wandId, String &action, String &entityId) {
	for (uint8_t i = 0; i < MAX_WAND_MAPPINGS; i++) {
		uint32_t storedWandId = preferences.getUInt(wandIdKey(i).c_str(), 0);
		if (storedWandId == wandId && storedWandId != 0) {
			action = preferences.getString(actionKey(i).c_str(), "");
			entityId = preferences.getString(entityKey(i).c_str(), "");
			if (action.length() > 0 && entityId.length() > 0) {
				return true;
			}
		}
	}
	return false;
}

inline bool isActionValid(const String &action) {
	return action == "toggle" || action == "turn_on" || action == "turn_off";
}

inline bool fetchHaLights(const String &serverName, const String &token, String &lightsJson, String &errorText) {
	String cleanedUrl = serverName;
	String cleanedToken = token;
	cleanedUrl.trim();
	cleanedToken.trim();
	if (cleanedUrl.endsWith("/")) {
		cleanedUrl.remove(cleanedUrl.length() - 1);
	}
	if (cleanedUrl.length() == 0 || cleanedToken.length() == 0) {
		errorText = "Empty Home Assistant URL or token";
		return false;
	}

	HTTPClient http;
	String apiURL = cleanedUrl;
	apiURL.concat("/api/template");

	http.setConnectTimeout(5000);
	http.setTimeout(10000);
	http.begin(apiURL.c_str());
	http.addHeader("Authorization", "Bearer " + cleanedToken);
	http.addHeader("Content-Type", "application/json");
	JSONVar reqBody;
	reqBody["template"] = "{{ states.light | map(attribute='entity_id') | join('\\n') }}";
	int httpResponseCode = http.POST(JSON.stringify(reqBody));

	if (httpResponseCode <= 0) {
		errorText = "Unable to reach Home Assistant: " + HTTPClient::errorToString(httpResponseCode);
		http.end();
		return false;
	}

	if (httpResponseCode != 200) {
		errorText = "Home Assistant returned HTTP " + String(httpResponseCode) + ": " + HTTPClient::errorToString(httpResponseCode);
		http.end();
		return false;
	}

	String payload = http.getString();
	http.end();

	payload.replace("\r", "");
	payload.trim();

	// /api/template may return a JSON string (quoted, with escaped newlines)
	// or plain text. Normalize both forms into newline-delimited text.
	if (payload.length() >= 2 && payload.charAt(0) == '"' && payload.charAt(payload.length() - 1) == '"') {
		payload = payload.substring(1, payload.length() - 1);
	}
	payload.replace("\\n", "\n");
	payload.replace("\\\"", "\"");
	Serial.print("[HA] template response chars: ");
	Serial.println(payload.length());

	JSONVar lightEntities;
	int outIndex = 0;
	int start = 0;
	while (start <= payload.length()) {
		int end = payload.indexOf('\n', start);
		if (end < 0) {
			end = payload.length();
		}
		String entityId = payload.substring(start, end);
		entityId.trim();
		if (entityId.length() > 0 && entityId.startsWith("light.")) {
			lightEntities[outIndex] = entityId;
			outIndex++;
		}
		if (end >= payload.length()) {
			break;
		}
		start = end + 1;
	}

	if (outIndex == 0) {
		errorText = "No light entities were found in Home Assistant";
		Serial.println("[HA] parsed light entities: 0");
		return false;
	}
	Serial.print("[HA] parsed light entities: ");
	Serial.println(outIndex);

	lightsJson = JSON.stringify(lightEntities);
	return true;
}

inline bool testHaConnection(const String &serverName, const String &token, String &errorText) {
	if (WiFi.status() != WL_CONNECTED) {
		errorText = "Device is not connected to WiFi";
		return false;
	}

	String cleanedUrl = serverName;
	String cleanedToken = token;
	cleanedUrl.trim();
	cleanedToken.trim();
	if (cleanedUrl.endsWith("/")) {
		cleanedUrl.remove(cleanedUrl.length() - 1);
	}
	if (cleanedUrl.length() == 0 || cleanedToken.length() == 0) {
		errorText = "Empty Home Assistant URL or token";
		return false;
	}

	HTTPClient http;
	String apiURL = cleanedUrl;
	apiURL.concat("/api/");

	http.setConnectTimeout(5000);
	http.setTimeout(7000);
	http.begin(apiURL.c_str());
	http.addHeader("Authorization", "Bearer " + cleanedToken);
	http.addHeader("Content-Type", "application/json");
	int httpResponseCode = http.GET();
	http.end();

	if (httpResponseCode == 200) {
		return true;
	}
	if (httpResponseCode < 0) {
		errorText = "HA test transport error " + String(httpResponseCode) + ": " + HTTPClient::errorToString(httpResponseCode);
	} else {
		errorText = "Home Assistant test failed with HTTP " + String(httpResponseCode) + " (check URL/token)";
	}
	return false;
}

inline void callHA() {
	const String serverName = preferences.getString("haurl", "1234");
	const String token = preferences.getString("token", "abcd");
	if (WiFi.status() == WL_CONNECTED) {
		HTTPClient http;
		String apiURL = serverName;
		JSONVar myObject;
		int wandID = IrGetWandID();
		String selectedAction;
		String selectedEntity;
		if (!getActionForWand(wandID, selectedAction, selectedEntity)) {
			return;
		}
		if (!isActionValid(selectedAction)) {
			return;
		}

		apiURL.concat("/api/states/");
		apiURL.concat(selectedEntity);
		myObject["entity_id"] = selectedEntity;
		Serial.print("----GET-----");
		Serial.println(apiURL.c_str());
		http.begin(apiURL.c_str());
		http.addHeader("Authorization", "Bearer " + token);
		http.addHeader("Content-Type", "application/json");
		int httpResponseCode = http.GET();
		if (httpResponseCode > 0) {
			Serial.print("HTTP Response code: ");
			Serial.println(httpResponseCode);
			String payload = http.getString();
			apiURL = serverName;

      if (selectedAction == "toggle") {
			  JSONVar response = JSON.parse(payload);
			  String state = response["state"];
			  if (state == "off") {
			  	apiURL.concat("/api/services/light/turn_on");
			  } else {
			  	apiURL.concat("/api/services/light/turn_off");
			  }
      } else if (selectedAction == "turn_on") {
        apiURL.concat("/api/services/light/turn_on");
      } else {
        apiURL.concat("/api/services/light/turn_off");
      }
			Serial.println(payload);
		} else {
			Serial.print("Error code: ");
			Serial.println(httpResponseCode);
			http.end();
			return;
		}
		http.end();

		Serial.print("----POST-----");
		Serial.println(apiURL.c_str());
		http.begin(apiURL.c_str());
		http.addHeader("Authorization", "Bearer " + token);
		http.addHeader("Content-Type", "application/json");
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
		http.end();
	} else {
		Serial.println("WiFi Disconnected");
	}
}

inline void loadHaConfig() {
	haConfig["haurl"] = (String)preferences.getString("haurl", "");
	haConfig["token"] = (String)preferences.getString("token", "");
	haConfig["ssid"] = (String)preferences.getString("rec_ssid", "") + " (RSSI: " + WiFi.RSSI() + " )";
	haConfig["password"] = (String)preferences.getString("rec_password", "");
	haConfig["hostname"] = (String)preferences.getString("hostname", hostname);
	haConfig["MAC"] = (String)WiFi.macAddress();
	haConfig["ip"] = (String)WiFi.localIP().toString();

	JSONVar mappings;
	int mapIndex = 0;
	for (uint8_t i = 0; i < MAX_WAND_MAPPINGS; i++) {
		uint32_t wandId = preferences.getUInt(wandIdKey(i).c_str(), 0);
		String action = preferences.getString(actionKey(i).c_str(), "");
		String entityId = preferences.getString(entityKey(i).c_str(), "");
		if (wandId != 0 && action.length() > 0 && entityId.length() > 0) {
			mappings[mapIndex]["wand_id"] = (double)wandId;
			mappings[mapIndex]["action"] = action;
			mappings[mapIndex]["entity_id"] = entityId;
			mapIndex++;
		}
	}
	haConfig["mappings"] = mappings;
}

inline String getHaConfigJson() {
	loadHaConfig();
	return JSON.stringify(haConfig);
}

#endif // HA_CLIENT_H
