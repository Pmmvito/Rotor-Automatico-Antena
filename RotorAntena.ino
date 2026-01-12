/*
 * Rotor de Antena VHF - ESP32-S3
 */

#include <WiFi.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include "config.h"
#include "encoder.h"
#include "motor_control.h"
#include "storage.h"
#include "web_server.h"
#include "ota_manager.h"

WiFiManager wm;  // Gerenciador WiFi

Encoder encoder(ENCODER_PIN_A, ENCODER_PIN_B, ENCODER_PPR, GEAR_RATIO);
StorageManager storage;
MotorController motorController(&encoder, &storage);
WebServerManager webServer(&motorController, &encoder, &storage);
OTAManager otaManager;

// Handle da tarefa de controle do motor
TaskHandle_t motorTaskHandle = NULL;

unsigned long lastPositionSave = 0;
unsigned long lastStatusBroadcast = 0;
const unsigned long POSITION_SAVE_INTERVAL = 5000;
const unsigned long STATUS_BROADCAST_INTERVAL = 100;  // 10x por segundo (sem enfileirar)

// Tarefa dedicada ao controle do motor e leitura do encoder (Core 0)
void motorTask(void *pvParameters) {
    Serial.print("Motor Task running on core ");
    Serial.println(xPortGetCoreID());
    
    // Loop infinito da tarefa
    for(;;) {
        // 1. Atualizar leitura do encoder (filtragem rapida)
        encoder.update();
        
        // 2. Atualizar controle do motor (PID)
        motorController.update();
        
        // Pequeno delay para evitar watchdog e permitir outras tarefas do sistema no Core 0
        // 1ms eh suficiente para ~1kHz de loop, o que eh otimo para PID
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("\n\n========================================");
    Serial.println("Rotor de Antena VHF - ESP32-S3");
    Serial.println("========================================");
    
    Serial.println("\n[1/5] Inicializando armazenamento...");
    if (!storage.begin()) {
        Serial.println("ERRO: Falha ao inicializar storage!");
    } else {
        Serial.println("Storage OK");
    }
    
    Serial.println("\n[2/5] Inicializando encoder...");
    encoder.begin();
    
    if (storage.hasCalibrationOffset()) {
        float offset = storage.loadCalibrationOffset();
        encoder.setCalibrationOffset(offset);
        Serial.print("Calibracao: ");
        Serial.println(offset);
    }
    
    Serial.println("\n[3/5] Inicializando motor...");
    motorController.begin();
    Serial.println("Motor OK");
    
    // Criar tarefa do motor no Core 0 (prioridade alta)
    xTaskCreatePinnedToCore(
        motorTask,          // Funcao da tarefa
        "MotorTask",        // Nome
        4096,               // Stack size (4KB deve ser suficiente)
        NULL,               // Parametros
        1,                  // Prioridade (1 = acima do Idle, mas cuidado com WiFi que roda no Core 0 as vezes)
        &motorTaskHandle,   // Handle
        0                   // Core 0
    );
    Serial.println("Tarefa de controle do motor iniciada no Core 0");
    
    if (storage.hasLastPosition()) {
        float lastPos = storage.loadLastPosition();
        Serial.print("Ultima posicao: ");
        Serial.print(lastPos);
        Serial.println(" graus");
        
        // Restaurar a posição como calibração do encoder
        encoder.setCalibrationOffset(lastPos);
        Serial.print("Offset de calibração restaurado: ");
        Serial.println(lastPos);
        
        // Restaurar posição absoluta acumulada
        float absPos = storage.loadAbsolutePosition();
        motorController.resetAbsolutePosition(absPos);
        Serial.print("Posicao absoluta restaurada: ");
        Serial.println(absPos);
        
        // Restaurar o último alvo
        float lastTarget = storage.loadLastTarget();
        if (lastTarget != 0.0) {
            motorController.moveToAngle(lastTarget);
            Serial.print("Alvo restaurado: ");
            Serial.println(lastTarget);
        }
    }
    
    Serial.println("\n[4/5] Conectando WiFi...");
    
    // Configurar WiFiManager
    wm.setConfigPortalTimeout(180); // Timeout de 3 minutos no portal
    wm.setConnectTimeout(20);       // Timeout de 20s para conectar
    wm.setHostname(WIFI_HOSTNAME);
    
    // Tentar conectar com credenciais salvas
    // Se falhar, abre portal de configuração "RotorAntena-Config" (sem senha)
    bool connected = wm.autoConnect("RotorAntena-Config");
    
    if (!connected) {
        Serial.println("Falha ao conectar WiFi!");
        Serial.println("Reiniciando para tentar novamente...");
        delay(3000);
        ESP.restart();
    }
    
    Serial.println("\nWiFi conectado!");
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    
    Serial.println("\n[5/5] Inicializando OTA e WebServer...");
    otaManager.begin();
    webServer.begin();
    
    Serial.println("\n========================================");
    Serial.println("Sistema pronto!");
    Serial.print("Acesse: http://");
    Serial.println(WiFi.localIP());
    Serial.println("========================================\n");
    
    float currentAngle = encoder.getAngle();
    Serial.print("Angulo atual: ");
    Serial.print(currentAngle);
    Serial.println(" graus");
}

void loop() {
    // Verificar se WiFi está conectado
    if (WiFi.status() == WL_CONNECTED) {
        otaManager.handle();
        webServer.update();
        
        // Enviar status via WebSocket periodicamente
        if (millis() - lastStatusBroadcast > STATUS_BROADCAST_INTERVAL) {
            webServer.broadcastStatus();
            lastStatusBroadcast = millis();
        }
    } else {
        // WiFi desconectado! Reabrir portal de configuração
        Serial.println("\n!!! WiFi desconectado! Abrindo portal de configuração...");
        wm.setConfigPortalTimeout(180);
        bool reconnected = wm.autoConnect("RotorAntena-Config", "rotor12345");
        
        if (!reconnected) {
            Serial.println("Falha ao reconectar. Reiniciando...");
            delay(3000);
            ESP.restart();
        } else {
            Serial.println("WiFi reconectado!");
            Serial.print("IP: ");
            Serial.println(WiFi.localIP());
        }
    }
    
    // motorController.update() removido daqui pois roda na tarefa dedicada
    
    if (millis() - lastPositionSave > POSITION_SAVE_INTERVAL) {
        if (motorController.isInMotion()) {
            float currentPos = encoder.getAngle();
            storage.saveLastPosition(currentPos);
        }
        lastPositionSave = millis();
    }
    
    delay(10);
}
