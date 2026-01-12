#include "motor_control.h"
#include "config.h"

MotorController::MotorController(Encoder* enc, StorageManager* store) 
    : encoder(enc), 
      storage(store),
      pinRPWM(MOTOR_RPWM), 
      pinLPWM(MOTOR_LPWM),
      pinREN(MOTOR_REN),
      pinLEN(MOTOR_LEN),
      pwmChannelR(0),
      pwmChannelL(1),
      currentPWM(0),
      targetPWM(0),
      currentDirection(MOTOR_STOP),
      targetDirection(MOTOR_STOP),
      targetAngle(0),
      targetAbsolutePosition(0),
      isMoving(false),
      isManualMode(false),
      lastUpdateTime(0),
      lastAccelTime(0) {
    mutex = xSemaphoreCreateMutex();
}

void MotorController::begin() {
    Serial.println("Inicializando motor...");
    
    pinMode(pinREN, OUTPUT);  // REN e LEN no mesmo pino
    pinMode(pinRPWM, OUTPUT);
    pinMode(pinLPWM, OUTPUT);
    
    digitalWrite(pinREN, LOW);
    digitalWrite(pinRPWM, LOW);
    digitalWrite(pinLPWM, LOW);
    
    Serial.println("  Configurando PWM...");
    
    ledcSetup(pwmChannelR, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(pwmChannelL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(pinRPWM, pwmChannelR);
    ledcAttachPin(pinLPWM, pwmChannelL);
    
    ledcWrite(pwmChannelR, 0);
    ledcWrite(pwmChannelL, 0);
    
    delay(100);
    
    digitalWrite(pinREN, HIGH);  // Ativar o driver (REN e LEN juntos)
    
    Serial.println("Motor controller OK (REN+LEN ligados juntos)");

    lastAngleDeg = encoder->getRawAngle(); // Usar getRawAngle que eh thread-safe agora
    
    // Carregar parâmetros aprendidos
    loadLearnedParameters();
}

void MotorController::setPWM(int pwm, MotorDirection direction) {
    currentDirection = direction;
    
    // Inverter direcao se configurado (compile-time)
    #if INVERT_MOTOR_DIRECTION
    if (direction == MOTOR_CW) {
        direction = MOTOR_CCW;
    } else if (direction == MOTOR_CCW) {
        direction = MOTOR_CW;
    }
    #endif
    
    // Inverter direcao se configurado (runtime)
    if (runtimeInvert) {
        if (direction == MOTOR_CW) {
            direction = MOTOR_CCW;
        } else if (direction == MOTOR_CCW) {
            direction = MOTOR_CW;
        }
    }
    
    if (direction == MOTOR_STOP || pwm == 0) {
        // Active Braking para BTS7960: Manter EN HIGH e PWM LOW
        // Isso curto-circuita o motor (Low-side MOSFETs ON)
        ledcWrite(pwmChannelR, 0);
        ledcWrite(pwmChannelL, 0);
        currentPWM = 0;
    } else if (direction == MOTOR_CW) {
        ledcWrite(pwmChannelR, pwm);
        ledcWrite(pwmChannelL, 0);
        currentPWM = pwm;
    } else if (direction == MOTOR_CCW) {
        ledcWrite(pwmChannelR, 0);
        ledcWrite(pwmChannelL, pwm);
        currentPWM = pwm;
    }
}

void MotorController::stop() {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        isMoving = false;
        isManualMode = false;
        targetPWM = 0;
        targetDirection = MOTOR_STOP;
        
        // Forcar parada imediata (Active Brake)
        setPWM(0, MOTOR_STOP);
        
        xSemaphoreGive(mutex);
        Serial.println("Motor stopping (Active Brake)...");
    }
}

float MotorController::calculateShortestPath(float current, float target) {
    float currentNorm = Encoder::normalizeAngle(current);
    float targetNorm = Encoder::normalizeAngle(target);
    
    float diff = targetNorm - currentNorm;
    
    // Garantir caminho mais curto (nunca mais de 180 graus)
    if (diff > 180.0) diff -= 360.0;
    if (diff < -180.0) diff += 360.0;
    
    return diff;
}

void MotorController::moveToAngle(float angle) {
    // Atualizar posição absoluta rastreada
    updateAbsolutePosition();
    
    // ============================================================
    // PROTEÇÃO CRÍTICA CONTRA TORÇÃO DE CABO
    // Se absolutePosition JÁ ultrapassou ±180° (por vento, drift, etc),
    // FORÇAR retorno pelo caminho seguro INDEPENDENTE do ângulo solicitado
    // ============================================================
    
    // Normalizar entrada para ±180°
    while (angle > 180.0) angle -= 360.0;
    while (angle < -180.0) angle += 360.0;
    
    Serial.printf("\n=== MOVE TO ANGLE DEBUG ===\n");
    Serial.printf("Solicitacao: %.1f\n", angle);
    Serial.printf("Pos Absoluta atual: %.1f\n", absolutePosition);
    
    // PROTEÇÃO 1: Se JÁ está fora do limite, permitir movimento direto
    if (absolutePosition > 180.0 || absolutePosition < -180.0) {
        Serial.printf("!!! ALERTA: Posicao absoluta FORA DO LIMITE (%.1f)! Movimento direto permitido.\n", absolutePosition);
        // Com movimento cru, permite ir diretamente para qualquer alvo válido
    }
    
    // Lógica normal de movimento
    
    // Posição alvo é igual ao ângulo solicitado
    float targetAbsPos = angle;
    
    // Calcular movimento necessário (CAMINHO MAIS CURTO - sempre usar posição real)
    float movement = targetAbsPos - absolutePosition;
    
    // NORMALIZAR movimento para caminho mais curto SEMPRE
    // Evita voltas desnecessárias mesmo quando fora do limite
    if (movement > 180.0) {
        movement -= 360.0;
        Serial.printf(">>> Normalizando movimento: %.1f° -> %.1f° (caminho mais curto)\n", targetAbsPos - absolutePosition, movement);
    } else if (movement < -180.0) {
        movement += 360.0;
        Serial.printf(">>> Normalizando movimento: %.1f° -> %.1f° (caminho mais curto)\n", targetAbsPos - absolutePosition, movement);
    }
    
    // PROTEÇÃO CONTRA TORÇÃO: Se movimento normalizado FARIA passar pelos limites, usar caminho alternativo
    float finalPosition = absolutePosition + movement;
    if (finalPosition > 180.0 || finalPosition < -180.0) {
        // Movimento normalizado levaria fora do limite - usar caminho longo seguro
        if (movement > 0) {
            movement -= 360.0;  // Usar CCW longo
        } else {
            movement += 360.0;  // Usar CW longo
        }
        Serial.printf(">>> PROTEÇÃO ATIVADA: Movimento ajustado para %.1f° (caminho longo para evitar torção)\n", movement);
    }
    
    Serial.printf("Movimento direto: %.1f\n", movement);
    Serial.printf("Posicao final se direto: %.1f\n", absolutePosition + movement);
    
    // REMOVIDA: Proteção 2 - A nova lógica de movimento cru já garante segurança
    Serial.printf("Movimento final: %.1f\n", movement);
    Serial.printf("Nova pos absoluta sera: %.1f\n", absolutePosition + movement);
    
    // Calcular ângulo do encoder correspondente
    float currentEncoderAngle = encoder->getAngle();
    float targetEncoderAngle = currentEncoderAngle + movement;
    
    Serial.printf("Encoder atual: %.1f\n", currentEncoderAngle);
    Serial.printf("Target Encoder (acumulado, nao normalizado): %.1f\n", targetEncoderAngle);
    
    // NÃO NORMALIZAR! Manter como ângulo acumulado para o PID saber qual caminho ir
    // A normalização acontece apenas para display/comparação
    
    Serial.printf("Movimento final: %.1f graus\n", movement);
    Serial.printf("Nova pos absoluta sera: %.1f\n", absolutePosition + movement);
    Serial.printf("========================\n\n");
    
    // Iniciar movimento
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        targetAbsolutePosition = absolutePosition + movement;  // NOVO: guardar alvo absoluto
        targetAngle = targetEncoderAngle;
        isMoving = true;
        pidIntegral = 0.0;
        pidLastError = 0.0;
        pidLastTime = millis();
        lastAngleDeg = encoder->getRawAngle();
        velDegPerSec = 0.0;
        xSemaphoreGive(mutex);
    }
}

int MotorController::calculatePID(float error, float dt) {
    // PID: output = Kp*error + Ki*integral + Kd*derivative
    
    // Termo Proporcional
    float P = KP * error;
    
    // Termo Integral (com anti-windup)
    if (abs(error) < 1.0f) {
        pidIntegral = 0.0f; // congela perto do alvo
    } else {
        pidIntegral += error * dt;
    }
    // Limitar integral para evitar windup
    float maxIntegral = PID_OUTPUT_LIMIT / (KI + 0.001);
    pidIntegral = constrain(pidIntegral, -maxIntegral, maxIntegral);
    float I = KI * pidIntegral;
    
    // Termo Derivativo
    float derivative = (error - pidLastError) / (dt + 0.001);
    float D = KD * derivative;
    
    // Saida total
    int output = (int)(P + I + D);
    
    // Limitar saida
    output = constrain(output, -PID_OUTPUT_LIMIT, PID_OUTPUT_LIMIT);
    
    // Salvar erro para proxima iteracao
    pidLastError = error;
    
    return abs(output); // Retornar valor absoluto (direcao ja eh tratada)
}

void MotorController::smoothAcceleration() {
    unsigned long currentTime = millis();
    
    // Controlar tempo entre passos de aceleracao
    if (currentTime - lastAccelTime < PWM_ACCEL_DELAY) {
        return;
    }
    lastAccelTime = currentTime;
    
    // Mudar direcao - primeiro desacelerar ate parar (usando DECEL step)
    if (targetDirection != currentDirection && currentPWM > 0) {
        currentPWM -= PWM_DECEL_STEP;
        if (currentPWM <= 0) {
            currentPWM = 0;
            currentDirection = targetDirection;
        }
        setPWM(currentPWM, currentDirection);
        return;
    }
    
    // Atualizar direcao se parado
    if (currentPWM == 0) {
        currentDirection = targetDirection;
    }
    
    // Acelerar ou desacelerar suavemente
    if (currentPWM < targetPWM) {
        currentPWM += PWM_ACCEL_STEP;
        if (currentPWM > targetPWM) currentPWM = targetPWM;
    } else if (currentPWM > targetPWM) {
        // Desaceleracao rapida para frear a inercia
        currentPWM -= PWM_DECEL_STEP;
        if (currentPWM < targetPWM) currentPWM = targetPWM;
    }
    
    // Garantir minimo quando em movimento
    if (targetPWM > 0 && currentPWM < PWM_MIN && currentPWM > 0) {
        currentPWM = PWM_MIN;
    }
    
    setPWM(currentPWM, currentDirection);
}

void MotorController::update() {
    unsigned long currentTime = millis();
    
    // Atualizar rastreamento de posição absoluta a cada ciclo
    updateAbsolutePosition();
    
    // Copiar variaveis compartilhadas para locais (thread-safe)
    bool localIsManualMode;
    bool localIsMoving;
    float localTargetAngle;
    float localTargetAbsolutePosition;
    int localSpeedPercent;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        localIsManualMode = isManualMode;
        localIsMoving = isMoving;
        localTargetAngle = targetAngle;
        localTargetAbsolutePosition = targetAbsolutePosition;
        localSpeedPercent = speedPercent;
        xSemaphoreGive(mutex);
    } else {
        return; // Se nao conseguir lock, tenta na proxima
    }
    
    // Modo manual - apenas suavizar aceleracao/desaceleracao
    if (localIsManualMode) {
        smoothAcceleration();
        return;
    }
    
    // Desacelerando apos parar
    if (!localIsMoving && currentPWM > 0) {
        smoothAcceleration();
        return;
    }
    
    // Modo automatico - ir para angulo
    if (!localIsMoving) return;
    
    // Sempre aplicar suavizacao
    if (currentTime - lastAccelTime >= PWM_ACCEL_DELAY) {
        smoothAcceleration();
    }
    
    // Atualizar logica de controle PID
    if (currentTime - lastUpdateTime < UPDATE_INTERVAL) {
        return;
    }
    
    // Calcular dt para PID
    float dt = (currentTime - lastUpdateTime) / 1000.0; // Converter para segundos
    lastUpdateTime = currentTime;
    
    float currentAngle = encoder->getRawAngle(); // Usar getRawAngle que eh rapido e thread-safe
    // Ajustar offset se necessario, mas para controle PID o raw + offset eh o que importa
    // O encoder->getRawAngle() retorna o angulo interno filtrado.
    // Precisamos somar o offset para comparar com o targetAngle que eh absoluto.
    currentAngle += encoder->getCalibrationOffset();
    // NÃO normalizar currentAngle! Manter como acumulado para comparar com targetAngle acumulado
    // currentAngle = Encoder::normalizeAngle(currentAngle);  // REMOVIDO
    
    // Calcular erro direto
    // Nota: erro pode ser > 180° se movimento foi deliberadamente longo!
    float error = localTargetAngle - currentAngle;
    
    // Apenas normalizar se error for maior que 540° (múltiplo de 360°)
    while (error > 540.0) error -= 360.0;
    while (error < -540.0) error += 360.0;
    
    float absError = abs(error);

    // Estimar velocidade angular filtrada
    float deltaAng = currentAngle - lastAngleDeg;
    // Detectar cruzamento de 0°/360° para cálculo de velocidade
    if (deltaAng > 180.0) deltaAng -= 360.0;
    if (deltaAng < -180.0) deltaAng += 360.0;
    
    float instVel = deltaAng / (dt + 0.001f);
    velDegPerSec = (0.8f * velDegPerSec) + (0.2f * instVel);
    lastAngleDeg = currentAngle;
    
    // Verificar chegada usando POSIÇÃO ABSOLUTA (para detecção correta com movimentos > 180°)
    float absPositionError = abs(absolutePosition - localTargetAbsolutePosition);
    
    // Chegou no alvo com precisao absoluta
    if (absPositionError < ANGLE_TOLERANCE && absError < ANGLE_TOLERANCE) {
        // Analisar overshoot para aprendizado
        analyzeOvershoot(currentAngle);
        
        Serial.printf("Chegou ao alvo! AbsPos: %.1f (target: %.1f), Encoder: %.1f (target: %.1f)\n",
                     absolutePosition, localTargetAbsolutePosition, currentAngle, localTargetAngle);
        
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            isMoving = false;
            targetPWM = 0;
            targetDirection = MOTOR_STOP;
            xSemaphoreGive(mutex);
        }
        pidIntegral = 0.0; // Resetar integral
        currentPWM = 0;
        setPWM(0, MOTOR_STOP);
        return;
    }

    // Se inverter sinal do erro, resetar integral para evitar overshoot
    if (error * pidLastError < 0) {
        pidIntegral = 0.0f;
    }
    
    // Calcular direcao (NUNCA MUDAR DIRECAO - ir direto)
    MotorDirection newDirection = (error > 0) ? MOTOR_CW : MOTOR_CCW;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        targetDirection = newDirection;
        xSemaphoreGive(mutex);
    }
    
    // Aplicar percentual de velocidade do usuario
    int maxPWM = (PWM_MAX * localSpeedPercent) / 100;
    if (maxPWM < PWM_MIN) maxPWM = PWM_MIN;
    
    int newTargetPWM = 0;
    
    // ==================================================================================
    // ESTRATEGIA HIBRIDA COM APRENDIZADO:
    // - Zona de Pulsos: < 2 graus (ajuste fino)
    // - Zona PID: 2 a 10 graus (aproximação controlada)
    // - Zona de Velocidade: > 10 graus (máxima velocidade)
    // - Frenagem Preditiva: baseada no aprendizado de inércia
    // ==================================================================================
    
    // Zona de Pulsos - Ajuste fino para alta precisão
    if (absError < 2.0) {
        // ZONA DE PULSOS (Micro-stepping PWM)
        // Sistema com alta inércia (1:5) - pulsos mais fracos e mais longos
        
        // Se erro < 0.3, considera alvo atingido (Deadband maior para alta inércia)
        if (absError < 0.3) {
            // Analisar overshoot para aprendizado
            analyzeOvershoot(currentAngle);
            
            // CORREÇÃO: Resetar absolutePosition para targetAbsolutePosition para evitar drift
            if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                absolutePosition = targetAbsolutePosition;  // Sincronizar posição absoluta
                isMoving = false;
                targetPWM = 0;
                targetDirection = MOTOR_STOP;
                xSemaphoreGive(mutex);
            }
            setPWM(0, MOTOR_STOP);
            pidIntegral = 0.0f;
            return;
        }
        
        // Ajustar intensidade do pulso para motor auto-travante
        // Motor helicoidal precisa pulsos fortes para vencer atrito
        int basePulsePWM = 180;  // Aumentado: engrenagem helicoidal tem muito atrito
        int adaptivePulsePWM = (int)(basePulsePWM / learnedInertiaFactor);
        adaptivePulsePWM = constrain(adaptivePulsePWM, 120, 220);  // Range maior para motor auto-travante
        
        // Ajustar duty cycle baseado no erro
        // Pulsos mais curtos para ajuste mais fino (reduzido 25%)
        int pulseOnTime = (int)(23 + (absError / 2.0) * 15);  // 23-38ms (reduzido 25%)
        pulseOnTime = constrain(pulseOnTime, 23, 45);
        
        // Gerador de Pulsos adaptativo
        unsigned long pulseCycle = millis() % 250;
        
        if (pulseCycle < (unsigned long)pulseOnTime) {
            // Fase ON: Pulso adaptativo
            MotorDirection pulseDir = (error > 0) ? MOTOR_CW : MOTOR_CCW;
            
            // Aplicar DIRETAMENTE (bypassing smoothAcceleration)
            setPWM(adaptivePulsePWM, pulseDir);
            
            // Atualizar estado interno para consistencia
            currentPWM = adaptivePulsePWM;
            currentDirection = pulseDir;
        } else {
            // Fase OFF: Freio/Parada para medir
            setPWM(0, MOTOR_STOP);
            currentPWM = 0;
        }
        
        // Retornar aqui para nao executar o resto da logica PID/Smooth
        return;
    }
    
    // ==================================================================================
    // ZONA PID / PROPORCIONAL (8 a 20 graus) - Motor auto-travante
    // ==================================================================================
    else if (absError < ZONE_MEDIUM) {
        // Gravar dados para aprendizado quando começar a desacelerar significativamente
        if (absError < 15.0 && absError > ZONE_SLOW && fabs(velDegPerSec) > 5.0) {
            recordApproachData(currentAngle, velDegPerSec);
        }
        
        int pidOutput = calculatePID(absError, dt);

        // Motor auto-travante precisa PWM mais alto para vencer atrito
        int pidMaxPWM = 450;  // Aumentado: engrenagem helicoidal tem muito atrito
        int pidMinPWM = 150;  // Aumentado: precisa vencer auto-travamento

        // Mapear saída PID para PWM
        newTargetPWM = map(pidOutput, 0, PID_OUTPUT_LIMIT, pidMinPWM, pidMaxPWM);
        newTargetPWM = constrain(newTargetPWM, pidMinPWM, pidMaxPWM);
    }
    // ==================================================================================
    // ZONA DE DESACELERAÇÃO PROGRESSIVA ( > 20 graus) - Frenagem muito gradual
    // ==================================================================================
    else {
        // Resetar integral do PID quando longe
        pidIntegral = 0.0;
        
        if (absError > ZONE_FAST) { // > 100 graus - máxima velocidade
            newTargetPWM = maxPWM;
        } 
        else if (absError > 75) { // 75-100 graus - Primeira redução suave
            newTargetPWM = (maxPWM * 85) / 100;  // Reduz apenas 15%
        }
        else if (absError > ZONE_MEDIUM) { // 50-75 graus - Segunda redução progressiva
            newTargetPWM = (maxPWM * 70) / 100;  // Reduz 30%
        }
        else if (absError > 35) { // 35-50 graus - Terceira redução
            newTargetPWM = (maxPWM * 55) / 100;  // Reduz 45%
        }
        else if (absError > ZONE_SLOW) { // 20-35 graus - Quarta redução (NOVO)
            newTargetPWM = (maxPWM * 40) / 100;  // Reduz 60%
        }
        else if (absError > ZONE_MEDIUM_SLOW) { // 5-20 graus - Quinta redução (NOVO)
            newTargetPWM = (maxPWM * 25) / 100;  // Reduz 75%
        }
        else {
            // < 5 graus: Última redução antes da zona de pulsos
            newTargetPWM = (maxPWM * 15) / 100;  // Reduz 85%
        }
    }
    
    // Garantir limites mínimos para motor auto-travante
    if (newTargetPWM > 0 && newTargetPWM < 100) {
        newTargetPWM = 100; // Motor helicoidal precisa PWM mínimo alto
    }
    if (newTargetPWM > maxPWM) {
        newTargetPWM = maxPWM;
    }

    // Frenagem antecipada se velocidade já é muito baixa próximo ao alvo
    if (absError < 0.3 && fabs(velDegPerSec) < 0.5) {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            targetPWM = 0;
            targetDirection = MOTOR_STOP;
            isMoving = false;
            xSemaphoreGive(mutex);
        }
        setPWM(0, MOTOR_STOP);
        pidIntegral = 0.0f;
        return;
    }
    
    // Atualizar targetPWM protegido
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        targetPWM = newTargetPWM;
        xSemaphoreGive(mutex);
    }
}

void MotorController::manualMove(int speed) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        isMoving = false;  // Cancelar modo automatico
        
        if (speed == 0) {
            // Soltar botao = desacelerar suavemente
            isManualMode = false;
            targetPWM = 0;
            targetDirection = MOTOR_STOP;
            xSemaphoreGive(mutex);
            return;
        }
        
        // Modo manual ativo
        isManualMode = true;
        targetDirection = (speed > 0) ? MOTOR_CW : MOTOR_CCW;
        
        // Usar velocidade configurada pelo usuario
        int maxPWM = (PWM_MAX * speedPercent) / 100;
        if (maxPWM < PWM_MIN) maxPWM = PWM_MIN;
        targetPWM = maxPWM;
        
        xSemaphoreGive(mutex);
        Serial.printf("Manual: %s @ %d%%\n", speed > 0 ? "CW" : "CCW", speedPercent);
    }
}

bool MotorController::isInMotion() {
    bool moving = false;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        moving = isMoving || isManualMode || currentPWM > 0;
        xSemaphoreGive(mutex);
    }
    return moving;
}

float MotorController::getTargetAngle() {
    float angle = 0.0f;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        angle = targetAngle;
        xSemaphoreGive(mutex);
    }
    return angle;
}

bool MotorController::hasReachedTarget() {
    bool reached = true;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (isMoving) {
            float currentAngle = encoder->getAngle(); // getAngle ja eh thread safe
            float error = calculateShortestPath(currentAngle, targetAngle);
            reached = (abs(error) < ANGLE_TOLERANCE);
        }
        xSemaphoreGive(mutex);
    }
    return reached;
}

void MotorController::setRuntimeInvert(bool invert) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        runtimeInvert = invert;
        xSemaphoreGive(mutex);
    }
}

bool MotorController::isRuntimeInverted() {
    bool inv = false;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        inv = runtimeInvert;
        xSemaphoreGive(mutex);
    }
    return inv;
}

void MotorController::setSpeedPercent(int percent) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        speedPercent = constrain(percent, 20, 100);
        xSemaphoreGive(mutex);
    }
}

int MotorController::getSpeedPercent() {
    int spd = 100;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        spd = speedPercent;
        xSemaphoreGive(mutex);
    }
    return spd;
}

// ==================================================================================
// SISTEMA DE APRENDIZADO ADAPTATIVO
// ==================================================================================

void MotorController::loadLearnedParameters() {
    if (storage && storage->hasLearnedParameters()) {
        learnedInertiaFactor = storage->loadInertiaFactor();
        learnedBrakingDist = storage->loadBrakingDistance();
        overshootAccumulator = storage->loadOvershootHistory();
        learningCycles = storage->loadLearningCycles();
        
        Serial.printf("=== APRENDIZADO CARREGADO ===\n");
        Serial.printf("  Fator Inércia: %.3f\n", learnedInertiaFactor);
        Serial.printf("  Dist. Frenagem: %.3f graus/(grau/s)\n", learnedBrakingDist);
        Serial.printf("  Overshoot médio: %.3f graus\n", overshootAccumulator);
        Serial.printf("  Ciclos: %d\n", learningCycles);
    } else {
        Serial.println("Sem dados de aprendizado. Usando defaults.");
        learnedInertiaFactor = 1.0;
        learnedBrakingDist = 0.1;
        overshootAccumulator = 0.0;
        learningCycles = 0;
    }
}

void MotorController::saveLearnedParameters() {
    if (storage) {
        storage->saveInertiaFactor(learnedInertiaFactor);
        storage->saveBrakingDistance(learnedBrakingDist);
        storage->saveOvershootHistory(overshootAccumulator);
        storage->saveLearningCycles(learningCycles);
        Serial.println("Parâmetros de aprendizado salvos.");
    }
}

void MotorController::resetLearning() {
    learnedInertiaFactor = 1.0;
    learnedBrakingDist = 0.1;
    overshootAccumulator = 0.0;
    overshootSamples = 0;
    learningCycles = 0;
    saveLearnedParameters();
    Serial.println("Aprendizado resetado!");
}

float MotorController::getInertiaFactor() {
    return learnedInertiaFactor;
}

float MotorController::getBrakingDistance() {
    return learnedBrakingDist;
}

int MotorController::getLearningCycles() {
    return learningCycles;
}

float MotorController::predictBrakingDistance(float velocity) {
    // Previsão baseada no aprendizado:
    // distância = velocidade * fator_frenagem * fator_inércia
    // 
    // O fator_frenagem (learnedBrakingDist) representa quantos graus 
    // o motor percorre durante a frenagem para cada grau/segundo de velocidade.
    //
    // O fator_inércia (learnedInertiaFactor) ajusta para motores mais pesados.
    
    float absVel = fabs(velocity);
    float predictedDist = absVel * learnedBrakingDist * learnedInertiaFactor;
    
    // Adicionar compensação baseada no overshoot histórico
    if (overshootAccumulator > 0 && learningCycles > 5) {
        // Se temos histórico de overshoot, compensar mais
        predictedDist += overshootAccumulator * 0.5;
    }
    
    return predictedDist;
}

void MotorController::recordApproachData(float currentAngle, float velocity) {
    // Chamado quando começamos a desacelerar para o alvo
    if (!isLearningApproach) {
        approachStartAngle = currentAngle;
        approachStartVel = velocity;
        approachStartTime = millis();
        isLearningApproach = true;
    }
}

void MotorController::analyzeOvershoot(float finalAngle) {
    // Chamado quando o motor parou completamente
    if (!isLearningApproach) return;
    
    isLearningApproach = false;
    
    // Calcular erro final (overshoot ou undershoot)
    float errorFromTarget = calculateShortestPath(finalAngle, targetAngle);
    float absError = fabs(errorFromTarget);
    
    // Se parou muito longe, não é um bom dado de aprendizado
    if (absError > 5.0) {
        return;
    }
    
    // Calcular a distância percorrida durante a frenagem
    float brakingDistance = fabs(calculateShortestPath(approachStartAngle, finalAngle));
    float approachVel = fabs(approachStartVel);
    
    // Evitar divisão por zero
    if (approachVel < 1.0) {
        return;
    }
    
    // Calcular fator de frenagem desta amostra
    float sampleBrakingFactor = brakingDistance / approachVel;
    
    // Determinar se houve overshoot (passou do alvo)
    // Se o sinal do erro mudou, houve overshoot
    float distToTarget = calculateShortestPath(approachStartAngle, targetAngle);
    bool wasOvershoot = (errorFromTarget * distToTarget) < 0;
    
    // Atualizar aprendizado com média móvel exponencial
    float alpha = 0.2;  // Taxa de aprendizado (20% da nova amostra)
    
    // Atualizar fator de frenagem
    learnedBrakingDist = (1.0 - alpha) * learnedBrakingDist + alpha * sampleBrakingFactor;
    
    // Atualizar fator de inércia baseado no overshoot
    if (wasOvershoot) {
        // Motor tem mais inércia que o esperado, aumentar fator
        learnedInertiaFactor = (1.0 - alpha) * learnedInertiaFactor + alpha * (learnedInertiaFactor * 1.1);
        overshootAccumulator = (1.0 - alpha) * overshootAccumulator + alpha * absError;
    } else if (absError > ANGLE_TOLERANCE) {
        // Motor parou antes (undershoot), diminuir fator levemente
        learnedInertiaFactor = (1.0 - alpha) * learnedInertiaFactor + alpha * (learnedInertiaFactor * 0.95);
    }
    
    // Limitar fatores para evitar valores extremos
    learnedInertiaFactor = constrain(learnedInertiaFactor, 0.5, 3.0);
    learnedBrakingDist = constrain(learnedBrakingDist, 0.01, 1.0);
    
    overshootSamples++;
    learningCycles++;
    
    // Salvar a cada 10 ciclos
    if (learningCycles % 10 == 0) {
        saveLearnedParameters();
    }
}

void MotorController::updateAbsolutePosition() {
    // Obter ângulo bruto do encoder (0-360°)
    float currentRaw = encoder->getRawAngle();
    
    // Na primeira chamada, apenas inicializar
    if (!absolutePositionInitialized) {
        lastRawAngleForTracking = currentRaw;
        absolutePositionInitialized = true;
        Serial.printf("Tracking absoluto inicializado: raw=%.1f | abs=%.1f\n", currentRaw, absolutePosition);
        return;
    }
    
    // Calcular diferença angular
    float delta = currentRaw - lastRawAngleForTracking;
    
    // Detectar cruzamento de fronteira 0°/360°
    if (delta > 180.0) {
        // Cruzou de 360° para 0° (sentido anti-horário)
        delta -= 360.0;
    } else if (delta < -180.0) {
        // Cruzou de 0° para 360° (sentido horário)
        delta += 360.0;
    }
    
    // Acumular na posição absoluta
    absolutePosition += delta;
    
    // Atualizar último ângulo
    lastRawAngleForTracking = currentRaw;
    
    // Verificar ultrapassagem de limite (alertar apenas uma vez)
    if (absolutePosition > 180.0) {
        if (!limitExceeded) {
            Serial.printf("\n!!! ALERTA CRITICO !!!\n");
            Serial.printf("Posicao absoluta %.1f ultrapassou +180 graus!\n", absolutePosition);
            Serial.printf("Direcao: HORARIA (CW / Direita)\n");
            Serial.printf("Cabo torcendo! Use botao 'Forcar Retorno' no site.\n\n");
            limitExceeded = true;
            limitExceededPositive = true;
        }
    } else if (absolutePosition < -180.0) {
        if (!limitExceeded) {
            Serial.printf("\n!!! ALERTA CRITICO !!!\n");
            Serial.printf("Posicao absoluta %.1f ultrapassou -180 graus!\n", absolutePosition);
            Serial.printf("Direcao: ANTI-HORARIA (CCW / Esquerda)\n");
            Serial.printf("Cabo torcendo! Use botao 'Forcar Retorno' no site.\n\n");
            limitExceeded = true;
            limitExceededPositive = false;
        }
    } else {
        // Dentro do limite - resetar flag
        if (limitExceeded) {
            Serial.println("Posicao absoluta voltou ao limite seguro.");
            limitExceeded = false;
        }
    }
}

float MotorController::getAbsolutePosition() {
    return absolutePosition;
}

void MotorController::resetAbsolutePosition(float pos) {
    absolutePosition = constrain(pos, -180.0, 180.0);
    lastRawAngleForTracking = encoder->getRawAngle();
    absolutePositionInitialized = false; // Forçar reinicialização no próximo update
    limitExceeded = false;               // Resetar flag de alerta
    limitExceededPositive = true;        // Resetar direção
    Serial.printf("Posicao absoluta resetada para: %.1f (tracking reinicializara)\n", absolutePosition);
}
