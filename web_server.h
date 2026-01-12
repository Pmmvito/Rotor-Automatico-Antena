#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "motor_control.h"
#include "encoder.h"
#include "storage.h"

class WebServerManager {
private:
    AsyncWebServer* server;
    AsyncWebSocket* ws;
    MotorController* motorController;
    Encoder* encoder;
    StorageManager* storage;
    
    // Flags de inversao runtime
    bool runtimeMotorInvert = false;
    bool runtimeEncoderInvert = false;
    
    void handleRoot(AsyncWebServerRequest *request);
    void handleStatus(AsyncWebServerRequest *request);
    void handleSetAngle(AsyncWebServerRequest *request);
    void handleManualControl(AsyncWebServerRequest *request);
    void handleCalibrate(AsyncWebServerRequest *request);
    void handleStop(AsyncWebServerRequest *request);
    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                          AwsEventType type, void *arg, uint8_t *data, size_t len);
    String getStatusJSON();
    String getHTMLPage();
    
public:
    WebServerManager(MotorController* motor, Encoder* enc, StorageManager* store);
    void begin();
    void update();
    void broadcastStatus();
    
    // Getters para inversao runtime
    bool isMotorInverted() { return runtimeMotorInvert; }
    bool isEncoderInverted() { return runtimeEncoderInvert; }
};

#endif
