#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>
#include <ESP32Encoder.h>
#include "config.h"

class Encoder {
private:
    ESP32Encoder encoder;
    int pinA;
    int pinB;
    float degreesPerPulse;
    float calibrationOffset;
    
    // Variáveis de filtro (movidas de estáticas para membros)
    long medBuf[3] = {0,0,0};
    uint8_t medIdx = 0;
    long movBuf[7] = {0,0,0,0,0,0,0};
    uint8_t movIdx = 0;
    bool filterInitialized = false;
    
    long lastFilteredCount;
    long lastRawCount = 0;
    bool runtimeInvert = false;
    
    // Variáveis protegidas
    float currentAngle = 0.0f;
    float velocityDegPerSec = 0.0f;
    SemaphoreHandle_t mutex;

public:
    Encoder(int pA, int pB, uint16_t ppr, float gearRatio);
    
    void begin();
    void update(); // Novo método para ser chamado na task rápida
    
    void setCalibrationOffset(float offset);
    float getCalibrationOffset();
    float getAngle(); // Thread-safe (apenas leitura)
    float getRawAngle(); // Agora retorna valor interno calculado no update()
    
    float getVelocityDegPerSec();
    static float normalizeAngle(float angle);
    void resetPosition();
    long getCount();
    
    void setRuntimeInvert(bool invert);
    bool isRuntimeInverted();
};

#endif
