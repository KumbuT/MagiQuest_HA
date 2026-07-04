#ifndef STORAGE_H
#define STORAGE_H

#include "Globals.h"

#include <LittleFS.h>
#include "FS.h"

inline void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
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

inline void logFsFileCheck(const char *path) {
	bool exists = LittleFS.exists(path);
	Serial.print("[FS] ");
	Serial.print(path);
	Serial.print(" -> ");
	Serial.println(exists ? "OK" : "MISSING");
}

inline void blink_LED(const char *mode, int duration = 5000) {
	int interval = 200;
	if (mode == NULL) mode = "SLOW";
	if (strcmp(mode, "SLOW") == 0) {
		interval = 50;
	} else {
		interval = 200;
	}

	unsigned long t = millis();
	while (millis() - t < (unsigned long)duration) {
		digitalWrite(LED_FEEDBACK_PIN, HIGH);
		delay(interval);
		digitalWrite(LED_FEEDBACK_PIN, LOW);
		delay(interval);
	}
}

inline void initStorage() {
	if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
		Serial.println("An Error has occurred while mounting LITTLEFS");
	}
	Serial.println("[FS] LittleFS mounted");
	logFsFileCheck("/index.html");
	logFsFileCheck("/sta-index.html");
	logFsFileCheck("/jquery-3.7.1.min.js");
	listDir(LittleFS, "/", 2);
	preferences.begin("my-pref", false);
	is_setup_done = preferences.getBool("is_setup_done", false);
	ssid = preferences.getString("rec_ssid", "Sample_SSID");
	password = preferences.getString("rec_password", "abcdefgh");
}

#endif // STORAGE_H
