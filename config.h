#ifndef CONFIG_H
#define CONFIG_H

// ========== WiFi ==========
#define WIFI_SSID "GIGAASTRUN"
#define WIFI_PASSWORD "Vitor2025@"
#define WIFI_HOSTNAME "RotorAntena"

// ========== Encoder MT6701 (Saidas A, B) ==========
#define ENCODER_PIN_A 4
#define ENCODER_PIN_B 5
#define ENCODER_PPR 4096
#define GEAR_RATIO 5.0
#define INVERT_ENCODER_DIRECTION true

// ========== Motor BTS7960 (ESP32-S3 compatible pins) ==========
#define MOTOR_RPWM 6
#define MOTOR_LPWM 7
#define MOTOR_EN 15              // REN e LEN ligados juntos
#define MOTOR_REN MOTOR_EN       // Mesmo pino
#define MOTOR_LEN MOTOR_EN       // Mesmo pino
#define INVERT_MOTOR_DIRECTION true

// ========== PWM Config (Otimizado para BTS7960 e Motor 12V @ 24V) ==========
#define PWM_FREQ 16000           // 16kHz
#define PWM_RESOLUTION 10        // 10 bits = 0-1023
// Motor 12V rodando em 24V precisa de PWM menor para nao queimar/overshoot
// IMPORTANTE: Motor com engrenagem helicoidal (auto-travante) precisa PWM mínimo alto
#define PWM_MIN 160              // Aumentado para vencer atrito e permitir pequenos movimentos
#define PWM_MAX 600              // Limitado a ~60% (aprox 14V efetivos) para proteger motor 12V
#define PWM_ACCEL_STEP 5         // Aceleração mais rápida (motor auto-travante não tem inércia livre)
#define PWM_DECEL_STEP 5         // Desaceleração igual (motor trava sozinho)
#define PWM_ACCEL_DELAY 15       // 15ms para transições

// ========== Controle PID para Precisao Absoluta ==========
// Motor auto-travante (engrenagem helicoidal) precisa ganhos mais altos
#define KP 2.5                   // Ganho proporcional
#define KI 0.01                  // Integral reduzido (evita oscillação)
#define KD 0.35                  // Damping maior para eliminar oscilação
#define PID_OUTPUT_LIMIT 600     // Acompanha PWM_MAX

// ========== Controle de Precisao (Sistema de Zonas) ==========
// Motor auto-travante: começar a freiar MUITO antes para distâncias longas
#define ZONE_FAST 100.0          // Começa desaceleração muito cedo (era 60)
#define ZONE_MEDIUM 50.0         // Segunda etapa de freio (era 30)
#define ZONE_SLOW 20.0           // Zona PID expandida (era 12)
#define ZONE_MEDIUM_SLOW 5.0     // Novo: Frenagem adicional antes do CRAWL
#define ZONE_CRAWL 0.5           
// Distribuição de PWM por zona:
// > 100° = PWM máximo com aceleração suave
// 50-100° = Reduz PWM linearmente (começa freio)
// 20-50° = PWM reduzido mais agressivamente
// 5-20° = PWM muito reduzido (frenagem fina)
// 0.5-5° = PWM proporcional decrescente
// 0.2-0.5° = pulsos curtos (60ms ON / 90ms OFF) - reduzido 25%
// < 0.2° = micro pulsos (37ms ON / 150ms OFF) - reduzido 25%

// ========== Behavior ==========
#define MAX_ANGLE 180.0
#define ANGLE_TOLERANCE 0.25     // Tolerancia de 0.25 graus (mais realista para motor com engrenagem)
#define UPDATE_INTERVAL 10       // Atualizar a cada 10ms (100Hz - mais responsivo)
#define SAVE_POSITION_INTERVAL 5000

// ========== Storage (NVS) ==========
#define CALIBRATION_KEY "calib"
#define POSITION_KEY "lastpos"

// ========== Web Server ==========
#define WEB_SERVER_PORT 80

// ========== Debug ==========
#define DEBUG_SERIAL true
#define SERIAL_BAUDRATE 115200

#endif
