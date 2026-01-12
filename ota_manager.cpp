#include "ota_manager.h"
#include <ArduinoOTA.h>
#include "config.h"

OTAManager::OTAManager() {}

void OTAManager::begin() {
    ArduinoOTA.setHostname(WIFI_HOSTNAME);
    ArduinoOTA.onStart([]() { Serial.println("OTA Start"); });
    ArduinoOTA.onEnd([]() { Serial.println("OTA End"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA Error[%u]\n", error);
    });
    ArduinoOTA.begin();
    Serial.println("[OTA] OK");
}

void OTAManager::handle() {
    ArduinoOTA.handle();
}