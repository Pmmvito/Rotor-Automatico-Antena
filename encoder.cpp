#include "encoder.h"
#include "config.h"

Encoder::Encoder(int pA, int pB, uint16_t ppr, float gearRatio)
    : pinA(pA), pinB(pB), calibrationOffset(0.0), lastFilteredCount(0) {
    degreesPerPulse = 360.0 / (ppr * gearRatio);
    mutex = xSemaphoreCreateMutex();
    
    Serial.println("[Encoder] Configuracao:");
    Serial.printf("  PPR: %d\n", ppr);
    Serial.printf("  Gear Ratio: %.1f:1\n", gearRatio);
    Serial.printf("  Graus/Pulso: %.6f\n", degreesPerPulse);
}

void Encoder::begin() {
    Serial.println("[Encoder] Iniciando com ESP32Encoder...");
    Serial.printf("  Pino A: %d\n", pinA);
    Serial.printf("  Pino B: %d\n", pinB);
    
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    
    // Usar Full Quad para detectar ambas as direcoes corretamente
    encoder.attachFullQuad(pinA, pinB);
    
    encoder.clearCount();
    
    Serial.println("[Encoder] Iniciado com Full Quadrature");
}

void Encoder::update() {
    long rawCount = encoder.getCount();

    // Inicializacao dos buffers de filtro
    if (!filterInitialized) {
        for (int i=0;i<3;i++) medBuf[i] = rawCount;
        for (int i=0;i<7;i++) movBuf[i] = rawCount;
        lastFilteredCount = rawCount;
        lastRawCount = rawCount;
        filterInitialized = true;
    }

    // Rejeitar saltos grosseiros (> ~300 pulsos ~5 graus) em um ciclo
    long rawDelta = rawCount - lastRawCount;
    lastRawCount = rawCount;
    if (abs(rawDelta) > 300) {
        rawCount = lastFilteredCount; // descarta espirro
    }

    // Filtro de mediana 3
    medBuf[medIdx] = rawCount;
    medIdx = (medIdx + 1) % 3;
    long a = medBuf[0], b = medBuf[1], c = medBuf[2];
    long med = (a + b + c) - min(a, min(b, c)) - max(a, max(b, c));

    // Média móvel de 7
    movBuf[movIdx] = med;
    movIdx = (movIdx + 1) % 7;
    long sum = 0;
    for (int i=0;i<7;i++) sum += movBuf[i];
    long filteredCount = sum / 7;

    long prevFiltered = lastFilteredCount;
    // Anti-jitter: travar se variação < 1 pulso
    long delta = abs(filteredCount - prevFiltered);
    if (delta < 1) {
        filteredCount = prevFiltered;
    } else {
        lastFilteredCount = filteredCount;
    }

    // Inverter se configurado (compile-time)
    #if INVERT_ENCODER_DIRECTION
    filteredCount = -filteredCount;
    #endif
    
    // Inverter se configurado (runtime)
    if (runtimeInvert) {
        filteredCount = -filteredCount;
    }
    
    float angle = filteredCount * degreesPerPulse;

    // Estimar velocidade angular filtrada (baixa ordem)
    static unsigned long lastVelTime = millis();
    unsigned long now = millis();
    float dt = (now - lastVelTime) / 1000.0f;
    if (dt <= 0) dt = 0.001f;
    float deltaDeg = (filteredCount - prevFiltered) * degreesPerPulse;
    float instVel = deltaDeg / dt;
    
    // Atualizar variaveis protegidas
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        velocityDegPerSec = (0.8f * velocityDegPerSec) + (0.2f * instVel);
        currentAngle = angle;
        xSemaphoreGive(mutex);
    }
    
    lastVelTime = now;
}

float Encoder::getAngle() {
    float angle = getRawAngle(); // Pega valor protegido
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        angle += calibrationOffset;
        xSemaphoreGive(mutex);
    } else {
        angle += calibrationOffset; // Fallback sem lock
    }
    return normalizeAngle(angle);
}

float Encoder::getRawAngle() {
    float angle = 0.0f;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        angle = currentAngle;
        xSemaphoreGive(mutex);
    }
    return angle;
}

float Encoder::getVelocityDegPerSec() {
    float vel = 0.0f;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        vel = velocityDegPerSec;
        xSemaphoreGive(mutex);
    }
    return vel;
}

float Encoder::normalizeAngle(float angle) {
    while (angle > 180.0) angle -= 360.0;
    while (angle <= -180.0) angle += 360.0;
    return angle;
}

void Encoder::setCalibrationOffset(float offset) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        calibrationOffset = offset;
        xSemaphoreGive(mutex);
        Serial.printf("[Encoder] Calibracao: %.2f graus\n", offset);
    }
}

float Encoder::getCalibrationOffset() {
    float offset = 0.0f;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        offset = calibrationOffset;
        xSemaphoreGive(mutex);
    }
    return offset;
}

void Encoder::resetPosition() {
    encoder.clearCount();
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        lastFilteredCount = 0;
        currentAngle = 0.0f;
        xSemaphoreGive(mutex);
    }
    Serial.println("[Encoder] Posicao resetada");
}

long Encoder::getCount() {
    return encoder.getCount();
}

void Encoder::setRuntimeInvert(bool invert) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        runtimeInvert = invert;
        xSemaphoreGive(mutex);
    }
}

bool Encoder::isRuntimeInverted() {
    bool inv = false;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        inv = runtimeInvert;
        xSemaphoreGive(mutex);
    }
    return inv;
}
