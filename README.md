# 📡 Controlador de Rotor de Antena - ESP32a


> **Sistema de posicionamento de precisão para antenas VHF/UHF com interface web, controle PID ## 📄 Termos de Uso (Licença)

Este projeto é disponibilizado gratuitamente para a comunidade de radioamadores e hobbistas.

- ✅ **Permitido**: Uso pessoal, educacional e modificações para uso próprio.
- ❌ **Proibido**: Uso comercial, venda de kits baseados neste código ou monetização direta/indireta sem autorização prévia.

## 🔌 API Reference (Para Integrações) proteção avançada contra torção de cabos.**

![Status](https://img.shields.io/badge/Status-Stable-success)
![Platform](https://img.shields.io/badge/Platform-ESP32--S3-blue)
![License](https://img.shields.io/badge/License-Personal_Use_Only-red)

## ⚡ Destaques do Projeto

- **🎯 Alta Precisão**: Encoder Magnético MT6701 (14-bit) com algoritmo de controle PID adaptativo.
- **🛡️ Segurança Ativa**: Sistema anti-torção com limites absolutos de ±180° e recuperação automática inteligente.
- **🌐 Interface Moderna**: Controle total via Browser (Mobile/Desktop) usando WebSocket em tempo real.
- **📶 Conectividade**: Configuração simplificada via WiFiManager (Portal Captivo) e suporte a mDNS.
- **💾 Persistência**: Salvamento automático de posição e calibração na memória NVS.

## 🛠️ Stack Tecnológico

| Componente | Especificação | Função |
|------------|---------------|--------|
| **MCU** | ESP32-S3 | Processamento Dual-core & WiFi |
| **Sensor** | MT6701 | Leitura de posição absoluta (I2C) |
| **Driver** | BTS7960 | Controle de potência do motor (43A) |
| **Motor** | Bosch FPG 12V 0 130 | Vidro Elétrico (Alto Torque/Redução) |

### Pinagem (Padrão)
- **I2C**: SDA (`GPIO 4`), SCL (`GPIO 5`)
- **Motor**: RPWM (`GPIO 6`), LPWM (`GPIO 7`), EN (`GPIO 15`)

## � Diagrama de Ligação

```mermaid
flowchart LR
    %% Definição de Estilos
    classDef power fill:#ffcc80,stroke:#e65100,stroke-width:2px,color:black
    classDef mcu fill:#b3e5fc,stroke:#0277bd,stroke-width:2px,color:black
    classDef driver fill:#ffcdd2,stroke:#c62828,stroke-width:2px,color:black
    classDef sensor fill:#e1bee7,stroke:#6a1b9a,stroke-width:2px,color:black
    classDef motor fill:#dcedc8,stroke:#558b2f,stroke-width:2px,color:black
    classDef ground fill:#cfd8dc,stroke:#455a64,stroke-width:2px,color:black

    %% --- BLOCO DE ALIMENTAÇÃO ---
    subgraph POWER [⚡ Fonte de Energia]
        direction TB
        VCC_12V(12V / 24V):::power
        GND(GND Comum):::ground
    end

    %% --- BLOCO CONTROLLER ---
    subgraph ESP32 [🧠 Microcontrolador ESP32-S3]
        direction TB
        ESP_3V3(3.3V Out):::mcu
        ESP_I2C(I2C: GPIO 4/5):::mcu
        ESP_PWM(PWM: GPIO 6/7):::mcu
        ESP_EN(Enable: GPIO 15):::mcu
    end

    %% --- BLOCO SENSOR ---
    subgraph MT6701 [🧭 Encoder MT6701]
        direction TB
        MT_PWR(VCC / GND):::sensor
        MT_DATA(SDA / SCL):::sensor
    end

    %% --- BLOCO DRIVER ---
    subgraph BTS7960 [🔌 Driver BTS7960]
        direction TB
        BTS_LOGIC(VCC / GND Lógico):::driver
        BTS_CTRL(RPWM / LPWM):::driver
        BTS_ENABLE(R_EN / L_EN):::driver
        BTS_POWER(B+ / B-):::driver
        BTS_OUT(M+ / M-):::driver
    end

    %% --- BLOCO MOTOR ---
    subgraph MECHANICAL [⚙️ Atuador]
        MOTOR((Motor Bosch)):::motor
    end

    %% CONEXÕES
    
    %% Energia Principal
    VCC_12V ==> BTS_POWER
    GND ==> BTS_POWER
    
    %% Energia Lógica
    ESP_3V3 --> MT_PWR
    ESP_3V3 --> BTS_LOGIC
    GND -.-> MT_PWR
    GND -.-> BTS_LOGIC

    %% Sinais de Controle (ESP -> Driver)
    ESP_PWM -->|PWM| BTS_CTRL
    ESP_EN -->|Ativação| BTS_ENABLE
    
    %% Sinais de Sensor (ESP <-> Encoder)
    ESP_I2C <-->|Comunicação I2C| MT_DATA

    %% Potência Motor (Driver -> Motor)
    BTS_OUT ==>|Alta Corrente| MOTOR
```

## �🚀 Quick Start

1. **Setup de Ambiente**
   - Instale VS Code + PlatformIO ou Arduino IDE.
   - Dependências: `WiFiManager`, `ESPAsyncWebServer`, `ArduinoJson`.

2. **Deploy**
   - Clone o projeto e realize o upload para o ESP32-S3.

3. **Configuração**
   - Conecte-se à rede WiFi gerada: `RotorAntena-Config`.
   - Configure sua rede WiFi local no portal.

4. **Uso**
   - Acesse `http://rotorantena.local` no navegador.
   - Aponte a antena para o Norte e clique em **"Calibrar Zero"**.

## 🧠 Lógica de Proteção (Anti-Torção)

O sistema implementa uma lógica estrita para proteger o cabeamento coaxial:

1. **Limites Absolutos**: O rotor opera estritamente entre -180° e +180°.
2. **Roteamento Inteligente**: 
   - Se o alvo está dentro do alcance direto: Vai pelo caminho mais curto.
   - Se o caminho curto causaria torção (ex: passar de +180°): O sistema inverte a rota automaticamente (caminho longo seguro).
3. **Recuperação de Vento**: Se forças externas moverem a antena para fora dos limites (ex: +181°), o sistema bloqueia movimentos inseguros e força o retorno para a zona segura.

## � Website Embarcado (Dashboard)

O sistema possui um **servidor web completo** rodando dentro do chip ESP32. Não é apenas uma API, mas uma interface gráfica rica e interativa.

A interface oferece:
- **🧭 Bússola em Tempo Real**: O ponteiro na tela segue exatamente o movimento da antena com animação fluida (WebSockets).
- **🎛️ Painel de Controle**: 
  - Slider de precisão para escolha de ângulo.
  - Botões táteis para ajuste fino manual (Esquerda/Direita).
  - Presets rápidos de direção.
- **📱 100% Responsivo**: Funciona como um aplicativo nativo no celular, tablet ou computador.

## �🔌 API Reference (Para Integrações)

Controle o rotor via HTTP para integrações (ex: N1MM, Ham Radio Deluxe):

- `GET /api/status` - Retorna JSON com telemetria completa.
- `POST /api/setangle` - Define azimute alvo (Payload: `angle=X`).
- `POST /api/stop` - Parada de emergência imediata.
- `POST /api/manual` - Controle manual de PWM.

---
*Desenvolvido para radioamadores exigentes. Código aberto para uso pessoal e não comercial.*