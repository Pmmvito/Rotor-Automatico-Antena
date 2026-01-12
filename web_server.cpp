#include "web_server.h"
#include "config.h"
#include "web_assets.h"

WebServerManager::WebServerManager(MotorController* motor, Encoder* enc, StorageManager* store)
    : motorController(motor), encoder(enc), storage(store) {
    server = new AsyncWebServer(WEB_SERVER_PORT);
    ws = new AsyncWebSocket("/ws");
}

void WebServerManager::begin() {
    ws->onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, 
                       AwsEventType type, void *arg, uint8_t *data, size_t len) {
        this->onWebSocketEvent(server, client, type, arg, data, len);
    });
    server->addHandler(ws);
    
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleRoot(request);
    });
    server->on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/css", STYLE_CSS);
    });
    server->on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "application/javascript", APP_JS);
    });
    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleStatus(request);
    });
    server->on("/api/setangle", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleSetAngle(request);
    });
    server->on("/api/manual", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleManualControl(request);
    });
    server->on("/api/calibrate", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleCalibrate(request);
    });
    server->on("/api/stop", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleStop(request);
    });
    server->begin();
    Serial.println("WebServer started");
}

void WebServerManager::update() { 
    // Cleanup sera feito no broadcastStatus para evitar duplicacao
}

void WebServerManager::onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                                        AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WS client #%u connected\n", client->id());
    // Enviar estado inicial ao cliente recém-conectado para evitar valores defasados
        String status = getStatusJSON();
        Serial.printf("Enviando status inicial: %s\n", status.c_str());
        client->text(status);
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WS client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            data[len] = 0;
            String msg = (char*)data;
            Serial.printf("WS received: %s\n", msg.c_str());
            
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, msg);
            if (!error) {
                if (doc.containsKey("angle")) {
                    float angle = doc["angle"];
                    // Motor fará validação e reroteamento automático
                    motorController->moveToAngle(angle);
                    Serial.printf("Moving to angle: %.1f\n", angle);
                }
                if (doc.containsKey("manual")) {
                    int speed = doc["manual"];
                    motorController->manualMove(speed);
                }
                if (doc.containsKey("stop")) {
                    motorController->stop();
                }
                if (doc.containsKey("calibrate")) {
                    float offset = -encoder->getRawAngle();
                    encoder->setCalibrationOffset(offset);
                    storage->saveCalibrationOffset(offset);
                    // Resetar posição absoluta para 0° (norte)
                    motorController->resetAbsolutePosition(0.0);
                    storage->saveAbsolutePosition(0.0);
                    Serial.println("Calibrated to North (absolute position reset to 0)");
                }
                if (doc.containsKey("forceRecovery")) {
                    // Forçar recuperação: mover para 0° (qualquer comando dispara recuperação automática)
                    Serial.println("RECUPERACAO FORCADA pelo usuario");
                    motorController->moveToAngle(0.0); // Tentar ir para 0°, recuperação automática ativará
                }
                if (doc.containsKey("invertMotor")) {
                    runtimeMotorInvert = doc["invertMotor"].as<bool>();
                    motorController->setRuntimeInvert(runtimeMotorInvert);
                    Serial.printf("Motor inversion: %s\n", runtimeMotorInvert ? "INVERTED" : "NORMAL");
                }
                if (doc.containsKey("invertEncoder")) {
                    runtimeEncoderInvert = doc["invertEncoder"].as<bool>();
                    encoder->setRuntimeInvert(runtimeEncoderInvert);
                    Serial.printf("Encoder inversion: %s\n", runtimeEncoderInvert ? "INVERTED" : "NORMAL");
                }
                // Controle de velocidade removido para simplificar
                
                // Comandos do sistema de aprendizado
                if (doc.containsKey("resetLearning")) {
                    motorController->resetLearning();
                    Serial.println("Motor learning reset!");
                }
                if (doc.containsKey("getLearning")) {
                    // Enviar status do aprendizado
                    StaticJsonDocument<256> resp;
                    resp["learningCycles"] = motorController->getLearningCycles();
                    resp["inertiaFactor"] = motorController->getInertiaFactor();
                    resp["brakingDist"] = motorController->getBrakingDistance();
                    String output;
                    serializeJson(resp, output);
                    client->text(output);
                }
            }
        }
    }
}

void WebServerManager::broadcastStatus() { 
    // Limpar fila antiga e enviar apenas se tiver espaco
    ws->cleanupClients();

    static unsigned long lastSend = 0;
    static bool wasMoving = false;
    unsigned long now = millis();
    bool isMoving = motorController->isInMotion();

    // Logica solicitada: "atualize o angulo do site somente apos finalizar o movimento"
    // Se estiver movendo, NAO envia updates (exceto talvez o primeiro para indicar inicio)
    // Se parou (borda de descida), envia update final.
    
    // Se esta movendo e ja estava movendo, ignora (silencio durante movimento)
    if (isMoving && wasMoving) {
        return;
    }

    // Se nao esta movendo e ja nao estava (idle), limita frequencia (heartbeat lento 1Hz)
    if (!isMoving && !wasMoving) {
        if (now - lastSend < 1000) return;
    }
    
    // Se mudou de estado (Parou->Andou ou Andou->Parou), envia imediatamente
    // Ou se eh o heartbeat
    
    // Se o motor parou (estava movendo e agora não está mais), salvar posição
    if (wasMoving && !isMoving) {
        float currentPos = encoder->getAngle();
        float absPos = motorController->getAbsolutePosition();
        storage->saveLastPosition(currentPos);
        storage->saveAbsolutePosition(absPos);
        Serial.printf("Movimento finalizado. Pos: %.1f | Abs: %.1f\n", currentPos, absPos);
    }
    
    if (ws->count() > 0) {
        if (ws->availableForWriteAll()) {
            ws->textAll(getStatusJSON());
            lastSend = now;
        }
    }
    
    wasMoving = isMoving;
}

String WebServerManager::getStatusJSON() {
    StaticJsonDocument<384> doc;  // Aumentado para incluir dados de aprendizado
    float currentAngle = encoder->getAngle();
    float targetAngle = motorController->getTargetAngle();
    float error = targetAngle - currentAngle;
    
    // Normalizar erro para -180 a 180
    while (error > 180.0) error -= 360.0;
    while (error <= -180.0) error += 360.0;
    
    // Normalizar erro para -180 a 180
    while (error > 180.0) error -= 360.0;
    while (error <= -180.0) error += 360.0;
    
    doc["angle"] = currentAngle;
    doc["target"] = targetAngle;
    doc["error"] = error;
    doc["moving"] = motorController->isInMotion();
    doc["calibration"] = encoder->getCalibrationOffset();
    doc["absolutePosition"] = motorController->getAbsolutePosition();
    
    // Dados do sistema de aprendizado
    doc["learning"]["cycles"] = motorController->getLearningCycles();
    doc["learning"]["inertia"] = serialized(String(motorController->getInertiaFactor(), 3));
    doc["learning"]["braking"] = serialized(String(motorController->getBrakingDistance(), 4));
    
    String output;
    serializeJson(doc, output);
    return output;
}

void WebServerManager::handleRoot(AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_HTML);
}

void WebServerManager::handleStatus(AsyncWebServerRequest *request) {
    request->send(200, "application/json", getStatusJSON());
}

void WebServerManager::handleSetAngle(AsyncWebServerRequest *request) {
    if (request->hasParam("angle", true)) {
        float angle = request->getParam("angle", true)->value().toFloat();
        motorController->moveToAngle(angle);
        storage->saveLastTarget(angle);  // Salvar alvo para restaurar após reboot
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    } else request->send(400, "application/json", "{\"error\":\"missing angle\"}");
}

void WebServerManager::handleManualControl(AsyncWebServerRequest *request) {
    if (request->hasParam("speed", true)) {
        int speed = request->getParam("speed", true)->value().toInt();
        motorController->manualMove(speed);
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    } else request->send(400, "application/json", "{\"error\":\"missing speed\"}");
}

void WebServerManager::handleCalibrate(AsyncWebServerRequest *request) {
    float offset = -encoder->getRawAngle();
    encoder->setCalibrationOffset(offset);
    storage->saveCalibrationOffset(offset);
    request->send(200, "application/json", "{\"status\":\"calibrated\"}");
}

void WebServerManager::handleStop(AsyncWebServerRequest *request) {
    motorController->stop();
    request->send(200, "application/json", "{\"status\":\"stopped\"}");
}

String WebServerManager::getHTMLPage() {
    return FPSTR(INDEX_HTML);
}
