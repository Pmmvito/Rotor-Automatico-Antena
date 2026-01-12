#ifndef MOTOR_CONTROL_H 
#define MOTOR_CONTROL_H

#include <Arduino.h>
#include "config.h"
#include "encoder.h"
#include "storage.h"

enum MotorDirection {
    MOTOR_STOP,
    MOTOR_CW,
    MOTOR_CCW
};

class MotorController {
private:
    uint8_t pinRPWM;
    uint8_t pinLPWM;
    uint8_t pinREN;
    uint8_t pinLEN;
    uint8_t pwmChannelR;
    uint8_t pwmChannelL;
    
    int currentPWM;
    int targetPWM;
    MotorDirection currentDirection;
    MotorDirection targetDirection;
    
    Encoder* encoder;
    StorageManager* storage;  // Para persistir aprendizado
    
    float targetAngle;
    float targetAbsolutePosition;  // Novo: posição absoluta desejada (±180°)
    bool isMoving;           // Modo automatico (ir para angulo)
    bool isManualMode;       // Modo manual (esquerda/direita)
    unsigned long lastUpdateTime;
    unsigned long lastAccelTime;  // Tempo para suavizacao
    
    bool runtimeInvert = false;  // Inversao runtime
    int speedPercent = 100;      // Velocidade 20-100%
    
    // Variaveis PID para controle preciso
    float pidIntegral = 0.0;     // Acumulador integral
    float pidLastError = 0.0;    // Erro anterior para derivada
    unsigned long pidLastTime = 0;

    // Estimativa de velocidade
    float lastAngleDeg = 0.0;
    float velDegPerSec = 0.0;
    
    // ==================================================================================
    // PROTEÇÃO CONTRA TORÇÃO DO CABO (±180° absoluto)
    // ==================================================================================
    float absolutePosition = 0.0;        // Posição absoluta acumulada (±180° limite)
    float lastRawAngleForTracking = 0.0; // Último ângulo raw para detectar voltas
    bool absolutePositionInitialized = false; // Flag: tracking inicializado?
    bool limitExceeded = false;          // Flag: limite ultrapassado? (evita spam de alertas)
    bool limitExceededPositive = true;   // true = ultrapassou +180°, false = -180°
    
    // ==================================================================================
    // SISTEMA DE APRENDIZADO ADAPTATIVO
    // ==================================================================================
    float learnedInertiaFactor = 1.0;    // Fator de inércia (1.0 = neutro)
    float learnedBrakingDist = 0.1;      // Graus de frenagem por grau/s de velocidade
    float overshootAccumulator = 0.0;    // Acumulador de overshoots
    int overshootSamples = 0;            // Número de amostras
    int learningCycles = 0;              // Total de ciclos aprendidos
    
    // Estado de aprendizado (para medir overshoot)
    float approachStartAngle = 0.0;      // Posição quando começou a desacelerar
    float approachStartVel = 0.0;        // Velocidade quando começou a desacelerar
    unsigned long approachStartTime = 0;
    bool isLearningApproach = false;     // Flag: está medindo overshoot?
    float lastStableAngle = 0.0;         // Última posição estável após parar
    
    SemaphoreHandle_t mutex;

    void setPWM(int pwm, MotorDirection direction);
    void smoothAcceleration();
    float calculateShortestPath(float current, float target);
    int calculatePID(float error, float dt);
    float predictBrakingDistance(float velocity);  // Previsão de frenagem baseada em aprendizado
    void recordApproachData(float currentAngle, float velocity);  // Gravar dados de aproximação
    void analyzeOvershoot(float finalAngle);  // Analisar overshoot e atualizar aprendizado
    
public:
    MotorController(Encoder* enc, StorageManager* store);
    void begin();
    void moveToAngle(float angle);
    void stop();
    void update();
    void manualMove(int speed);
    bool isInMotion();
    float getTargetAngle();
    bool hasReachedTarget();
    
    // Proteção contra torção do cabo
    void updateAbsolutePosition();          // Atualizar posição absoluta rastreada
    float getAbsolutePosition();            // Obter posição absoluta (±180° limite)
    void resetAbsolutePosition(float pos);  // Resetar posição absoluta (calibração)
    
    // Inversao runtime
    void setRuntimeInvert(bool invert);
    bool isRuntimeInverted();
    
    // Controle de velocidade
    void setSpeedPercent(int percent);
    int getSpeedPercent();
    
    // Sistema de Aprendizado
    void loadLearnedParameters();           // Carregar parâmetros do storage
    void saveLearnedParameters();           // Salvar parâmetros no storage
    void resetLearning();                   // Resetar aprendizado
    float getInertiaFactor();               // Obter fator de inércia atual
    float getBrakingDistance();             // Obter distância de frenagem aprendida
    int getLearningCycles();                // Quantos ciclos já aprendeu
};

#endif
