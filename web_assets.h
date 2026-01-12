#pragma once

// Assets do frontend servidos a partir de PROGMEM para facilitar manutencao
// Dependencias Arduino para PROGMEM
#include <Arduino.h>
// Arquivo separado para HTML/CSS/JS do painel Web

// HTML principal
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Rotor de Antena VHF</title>
    <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"/>
    <link rel="stylesheet" href="/style.css" />
    <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
</head>
<body>
<div class="container">
    <h1>Rotor de Antena VHF</h1>
    <div class="subtitle">Controle de Posicionamento</div>
    
    <div class="card">
        <div class="card-title">Posicao Atual</div>
        <div class="angle-display">
            <span class="angle-big" id="currentAngle">0.0</span><span style="font-size:1.5em">&deg;</span>
        </div>
        <div style="margin-top:10px; font-size:0.9em; color:var(--muted)">
            Erro: <span id="errorDisplay" style="color:var(--accent); font-weight:bold">0.00°</span>
        </div>
        <div style="margin-top:5px; font-size:0.85em; color:var(--muted)">
            Pos. Absoluta: <span id="absolutePosition" style="font-weight:bold">0.0°</span>
            <span id="cableWarning" style="color:#ff3333; font-weight:bold; display:none"> ⚠ LIMITE!</span>
        </div>
        <div class="status" id="status">Parado</div>
    </div>
    
    <div class="card">
        <div class="card-title">Ajuste Fino</div>
        <div class="fine-btns">
            <button class="fine-btn" onclick="adjust(-10)">-10</button>
            <button class="fine-btn" onclick="adjust(-1)">-1</button>
            <button class="fine-btn" onclick="adjust(1)">+1</button>
            <button class="fine-btn" onclick="adjust(10)">+10</button>
        </div>
        <div class="input-row">
            <input type="number" id="targetAngle" min="0" max="360" step="0.1" value="0">
            <button class="btn btn-primary" onclick="setAngle()">IR</button>
        </div>
    </div>
    
    <div class="card">
        <div class="card-title">Direcoes Rapidas</div>
        <div class="presets">
            <div class="preset" onclick="goTo(0)"><span class="d">N</span><br>0</div>
            <div class="preset" onclick="goTo(45)"><span class="d">NE</span><br>45</div>
            <div class="preset" onclick="goTo(90)"><span class="d">L</span><br>90</div>
            <div class="preset" onclick="goTo(135)"><span class="d">SE</span><br>135</div>
            <div class="preset" onclick="goTo(180)"><span class="d">S</span><br>180</div>
            <div class="preset" onclick="goTo(225)"><span class="d">SO</span><br>225</div>
            <div class="preset" onclick="goTo(270)"><span class="d">O</span><br>270</div>
            <div class="preset" onclick="goTo(315)"><span class="d">NO</span><br>315</div>
        </div>
    </div>
    
    <div class="card">
        <div class="card-title">Mapa - Clique para Mirar / Arraste a Torre</div>
        <div class="map-controls">
            <button class="btn btn-secondary" onclick="zoomIn()">+</button>
            <button class="btn btn-secondary" onclick="zoomOut()">-</button>
            <button class="btn btn-secondary" onclick="centerMap()">Centralizar</button>
        </div>
        <div id="map"></div>
        <div class="map-info">
            <div>Torre: <span class="val" id="towerCoords">-22.8, -47.0</span></div>
            <div>Alvo: <span class="val" id="targetCoords">-</span></div>
            <div>Azimute: <span class="val" id="azimuth">-</span></div>
            <div>Distancia: <span class="val" id="distance">-</span></div>
        </div>
    </div>
    
    <div class="card">
        <div class="card-title">Calibracao e Configuracao</div>
        <button class="btn btn-secondary" onclick="calibrate()" style="width:100%; margin-bottom:10px">Definir Norte (0 graus)</button>
        <button class="btn" onclick="forceRecovery()" style="width:100%; margin-bottom:10px; background:#ff6600">Forcar Retorno ao Limite Seguro</button>
        
        <div class="card-title" style="margin-top:15px">Inverter Direcoes</div>
        <div style="display:flex; gap:10px; margin-top:10px">
            <button class="btn btn-secondary" onclick="toggleMotor()" style="flex:1">
                <span id="motorInvert">Motor: Normal</span>
            </button>
            <button class="btn btn-secondary" onclick="toggleEncoder()" style="flex:1">
                <span id="encoderInvert">Encoder: Normal</span>
            </button>
        </div>
        <div style="font-size:0.8em; color:var(--muted); margin-top:8px; text-align:center">
            Use se o motor girar ao contrario ou se o angulo aumentar/diminuir invertido
        </div>
    </div>
    
    <div class="footer">Rotor VHF ESP32-S3 - OTA Ativo</div>
</div>

<script src="/app.js"></script>
</body>
</html>
)rawliteral";

// CSS separado
const char STYLE_CSS[] PROGMEM = R"rawliteral(
* { box-sizing: border-box; margin: 0; padding: 0; }
:root {
    --primary: #00d4ff; --secondary: #7c3aed; --danger: #ef4444; --success: #22c55e;
    --bg: #0f0f23; --card: rgba(255,255,255,0.05); --text: #fff; --muted: #94a3b8;
}
body { font-family: system-ui, sans-serif; background: var(--bg); color: var(--text); padding: 10px; }
.container { max-width: 500px; margin: 0 auto; }
h1 { text-align: center; color: var(--primary); font-size: 1.4em; margin-bottom: 5px; }
.subtitle { text-align: center; color: var(--muted); font-size: 0.8em; margin-bottom: 15px; }
.card { background: var(--card); border-radius: 15px; padding: 15px; margin-bottom: 12px; }
.card-title { font-size: 0.7em; text-transform: uppercase; color: var(--muted); margin-bottom: 10px; 
              padding-left: 8px; border-left: 3px solid var(--primary); }

.angle-display { text-align: center; margin: 25px 0; }
.angle-big { font-size: 4em; font-weight: bold; color: var(--primary); }
.status { text-align: center; font-size: 0.9em; color: var(--success); padding: 8px; }
.status.moving { color: var(--primary); animation: pulse 1s infinite; }
@keyframes pulse { 50% { opacity: 0.5; } }
@keyframes blink { 0%, 100% { opacity: 1; } 50% { opacity: 0.3; } }

.fine-btns { display: flex; gap: 5px; flex-wrap: wrap; justify-content: center; margin: 10px 0; }
.fine-btn { padding: 12px 16px; border: 1px solid rgba(255,255,255,0.2); border-radius: 8px;
    background: var(--card); color: var(--text); font-size: 1em; cursor: pointer; }
.fine-btn:active { background: var(--primary); color: #000; }

.input-row { display: flex; gap: 8px; margin-top: 10px; }
.input-row input { flex: 1; padding: 14px; border: 1px solid rgba(255,255,255,0.2); border-radius: 8px;
    background: rgba(0,0,0,0.3); color: var(--text); font-size: 1.2em; text-align: center; }
.input-row input:focus { outline: none; border-color: var(--primary); }

.btn { padding: 14px 20px; border: none; border-radius: 8px; font-weight: bold; cursor: pointer; font-size: 1em; }
.btn-primary { background: var(--primary); color: #000; }
.btn-danger { background: var(--danger); color: #fff; }
.btn-secondary { background: rgba(255,255,255,0.1); color: var(--text); }
.btn:active { transform: scale(0.95); }

.presets { display: grid; grid-template-columns: repeat(4, 1fr); gap: 6px; margin: 10px 0; }
.preset { padding: 12px 5px; border: 1px solid rgba(255,255,255,0.1); border-radius: 8px;
    background: var(--card); text-align: center; cursor: pointer; font-size: 0.85em; }
.preset:active { border-color: var(--primary); }
.preset .d { color: var(--primary); font-weight: bold; }

.manual-btns { display: flex; gap: 10px; justify-content: center; margin: 15px 0; }
.manual-btns button { flex: 1; padding: 16px 8px; font-size: 0.9em; white-space: nowrap; }
.manual-btns .btn-danger { flex: 0 0 auto; min-width: 80px; }

#map { height: 300px; border-radius: 12px; margin: 10px 0; }
.map-controls { display: flex; gap: 8px; margin-bottom: 10px; flex-wrap: wrap; }
.map-controls button { flex: 1; min-width: 80px; }
.map-info { background: rgba(0,0,0,0.5); padding: 12px; border-radius: 8px; font-size: 0.9em; }
.map-info .val { color: var(--primary); font-weight: bold; }
.gps-status { font-size: 0.8em; color: var(--muted); margin-top: 5px; }

.leaflet-popup-content-wrapper { background: #1a1a2e; color: #fff; }
.leaflet-popup-tip { background: #1a1a2e; }
.footer { text-align: center; padding: 15px; color: var(--muted); font-size: 0.7em; }
)rawliteral";

// JavaScript separado
const char APP_JS[] PROGMEM = R"rawliteral(
let motorInverted = false;
let encoderInverted = false;
let ws;
let currentAngle = 0;
let absolutePosition = 0; // Posição absoluta rastreada
let map, towerMarker, targetMarker, line;
let towerPos = {lat: -22.8, lng: -47.0};
let targetPos = null;

function connect() {
    ws = new WebSocket('ws://' + location.host + '/ws');
    ws.onopen = function() {
        // Enviar configuracoes salvas apos conexao
        loadInvertSettings();
    };
    ws.onmessage = function(e) {
        try {
            let d = JSON.parse(e.data);
            
            // Se temos a chave 'angle', atualizar a posição
            if (d.angle !== undefined) {
                currentAngle = d.angle; // Manter interno como ±180°
                
                // Converter para 0-360° para exibição visual
                let displayAngle = d.angle;
                if (displayAngle < 0) {
                    displayAngle += 360.0; // Ex: -150° vira 210°
                }
                
                // Atualizar angulo atual com 2 casas decimais (0-360°)
                let angleElement = document.getElementById('currentAngle');
                if (angleElement) {
                    angleElement.textContent = displayAngle.toFixed(2);
                }
            }
            
            // Mostrar erro com 3 casas decimais
            if (d.error !== undefined) {
                let errorAbs = Math.abs(d.error);
                let errorColor = errorAbs < 0.1 ? '#00ff00' : (errorAbs < 0.5 ? '#ffaa00' : '#ff3333');
                let errorEl = document.getElementById('errorDisplay');
                if (errorEl) {
                    errorEl.textContent = d.error.toFixed(3) + '°';
                    errorEl.style.color = errorColor;
                }
            }
            
            // Mostrar posição absoluta e avisar se próximo do limite
            if (d.absolutePosition !== undefined) {
                absolutePosition = d.absolutePosition; // Atualizar variável global
                let absPos = d.absolutePosition;
                let absPosEl = document.getElementById('absolutePosition');
                let warningEl = document.getElementById('cableWarning');
                
                if (absPosEl) {
                    // Colorir baseado na proximidade do limite
                    let absValue = Math.abs(absPos);
                    let color = absValue > 180 ? '#ff0000' : (absValue > 170 ? '#ff3333' : (absValue > 150 ? '#ffaa00' : '#00ff00'));
                    absPosEl.textContent = absPos.toFixed(1) + '°';
                    absPosEl.style.color = color;
                }
                
                // Mostrar alerta CRÍTICO se ultrapassar ±180° (FORA DO LIMITE!)
                if (warningEl) {
                    let absValue = Math.abs(absPos);
                    if (absValue > 180) {
                        warningEl.textContent = ' 🚨 LIMITE ULTRAPASSADO! RECUPERAÇÃO AUTOMÁTICA';
                        warningEl.style.display = 'inline';
                        warningEl.style.color = '#ff0000';
                        warningEl.style.animation = 'blink 1s infinite';
                    } else if (absValue > 160) {
                        warningEl.textContent = ' ⚠ PRÓXIMO DO LIMITE!';
                        warningEl.style.display = 'inline';
                        warningEl.style.color = '#ff6600';
                        warningEl.style.animation = 'none';
                    } else {
                        warningEl.style.display = 'none';
                    }
                }
            }
            
            let st = document.getElementById('status');
            if (st && d.moving !== undefined) {
                if (d.moving) { st.textContent = 'Movendo...'; st.className = 'status moving'; }
                else { st.textContent = 'Parado'; st.className = 'status'; }
            }
        } catch(err) { console.error('Erro ao processar mensagem:', err); }
    };
    ws.onclose = function() { setTimeout(connect, 2000); };
    ws.onerror = function() { ws.close(); };
}

function send(o) { 
    if (ws && ws.readyState === 1) {
        ws.send(JSON.stringify(o)); 
        return true;
    }
    return false;
}
function adjust(v) { 
    // Converter currentAngle (0-360°) para ±180° antes de ajustar
    let internalAngle = angleToInternal(currentAngle);
    
    // Calcular novo ângulo (motor fará a validação e reroteamento)
    let newAngle = internalAngle + v;
    while (newAngle > 180.0) newAngle -= 360.0;
    while (newAngle < -180.0) newAngle += 360.0;
    
    if (send({angle: newAngle})) {
        // Converter para 0-360° para exibição
        let displayAngle = newAngle < 0 ? newAngle + 360.0 : newAngle;
        document.getElementById('targetAngle').value = displayAngle.toFixed(1);
        document.getElementById('status').textContent = 'Indo para ' + displayAngle.toFixed(1) + '...';
        document.getElementById('status').className = 'status moving';
    }
}

// Converter 0-360° para ±180°, mantendo o AZIMUTE CORRETO
function angleToInternal(displayAngle) {
    // Normalizar entrada para 0-360°
    while (displayAngle < 0) displayAngle += 360.0;
    while (displayAngle >= 360) displayAngle -= 360.0;
    
    // Se > 180°, subtrair 360° para obter o equivalente negativo
    // Ex: 190° → -170° (mesmo azimute, movimento gigante de volta)
    // Motor volta 170° completos para ficar em -170° (que é 190°!)
    if (displayAngle > 180.0) {
        return displayAngle - 360.0;
    }
    return displayAngle;
}

// Validar se o movimento respeita a proteção de torção (±180°)
function validateMovement(targetAngle) {
    // currentAngle está em 0-360°
    let internalCurrent = angleToInternal(currentAngle);
    let internalTarget = angleToInternal(targetAngle);
    
    // Calcular movimento
    let movement = internalTarget - internalCurrent;
    
    // Verificar se ultrapassaria ±180°
    let finalPos = internalCurrent + movement;
    
    if (finalPos > 180.0 || finalPos < -180.0) {
        // Seria bloqueado, informar usuário
        alert('❌ BLOQUEADO: Movimento ultrapassaria o limite de proteção de cabo (±180°).\n' +
              'Motor vai tentar redirecionar para o caminho seguro.');
        return false;
    }
    return true;
}

function goTo(a) {
    // Converter 0-360° para ±180° com caminho mais curto
    let internalAngle = angleToInternal(a);
    
    // Motor fará validação e reroteamento automático
    if (send({angle: internalAngle})) {
        // Exibir como 0-360°
        let displayAngle = a < 0 ? a + 360.0 : a;
        if (displayAngle >= 360) displayAngle -= 360.0;
        
        document.getElementById('targetAngle').value = displayAngle.toFixed(1);
        document.getElementById('status').textContent = 'Indo para ' + displayAngle.toFixed(1) + '...';
        document.getElementById('status').className = 'status moving';
        localStorage.setItem('lastTargetAngle', displayAngle.toString());
    }
}

function setAngle() { 
    let angle = parseFloat(document.getElementById('targetAngle').value);
    if (isNaN(angle)) {
        alert('Angulo invalido!');
        return;
    }
    
    // Validar intervalo 0-360°
    if (angle < 0 || angle > 360) {
        alert('Digite um angulo entre 0 e 360 graus!');
        return;
    }
    
    // Converter 0-360° para ±180° com caminho mais curto
    let internalAngle = angleToInternal(angle);
    
    // Motor fará validação e reroteamento automático
    if (send({angle: internalAngle})) {
        document.getElementById('status').textContent = 'Indo para ' + angle.toFixed(1) + '...';
        document.getElementById('status').className = 'status moving';
        localStorage.setItem('lastTargetAngle', angle.toString());
    }
}
function stopMotor() { 
    if (send({stop: true})) {
        document.getElementById('status').textContent = 'Parado';
        document.getElementById('status').className = 'status';
    }
}
function calibrate() { if(confirm('Definir posicao atual como Norte (0 graus)?')) send({calibrate: true}); }

function forceRecovery() {
    if(confirm('FORCAR retorno ao limite seguro (±175 graus)? Use apenas se o cabo estiver torcido!')) {
        send({forceRecovery: true});
        document.getElementById('status').textContent = 'Recuperando posicao segura...';
        document.getElementById('status').className = 'status moving';
    }
}

function toggleMotor() {
    motorInverted = !motorInverted;
    send({invertMotor: motorInverted});
    document.getElementById('motorInvert').textContent = 
        'Motor: ' + (motorInverted ? 'Invertido' : 'Normal');
    localStorage.setItem('motorInverted', motorInverted);
}

function toggleEncoder() {
    encoderInverted = !encoderInverted;
    send({invertEncoder: encoderInverted});
    document.getElementById('encoderInvert').textContent = 
        'Encoder: ' + (encoderInverted ? 'Invertido' : 'Normal');
    localStorage.setItem('encoderInverted', encoderInverted);
}

function loadInvertSettings() {
    motorInverted = localStorage.getItem('motorInverted') === 'true';
    encoderInverted = localStorage.getItem('encoderInverted') === 'true';
    
    // Sempre atualizar texto dos botoes
    document.getElementById('motorInvert').textContent = 
        'Motor: ' + (motorInverted ? 'Invertido' : 'Normal');
    document.getElementById('encoderInvert').textContent = 
        'Encoder: ' + (encoderInverted ? 'Invertido' : 'Normal');
    
    // Enviar estado para o ESP32
    if (motorInverted) {
        send({invertMotor: true});
    }
    if (encoderInverted) {
        send({invertEncoder: true});
    }
}

function loadLastAngle() {
    try {
        let savedAngle = localStorage.getItem('lastTargetAngle');
        
        if (savedAngle !== null && savedAngle !== '' && savedAngle !== 'null') {
            let angle = parseFloat(savedAngle);
            if (!isNaN(angle)) {
                document.getElementById('targetAngle').value = angle.toFixed(1);
            } else {
                document.getElementById('targetAngle').value = '0';
            }
        } else {
        }
    } catch(e) {
        console.error('Erro ao carregar angulo:', e);
    }
}

function loadLastPosition() {
    try {
        let savedAngle = localStorage.getItem('lastTargetAngle');
        
        if (savedAngle !== null && savedAngle !== '' && savedAngle !== 'null') {
            let angle = parseFloat(savedAngle);
            if (!isNaN(angle)) {
                document.getElementById('targetAngle').value = angle.toFixed(1);
                // Converter 0-360° para ±180° antes de enviar
                let internalAngle = angleToInternal(angle);
                send({angle: internalAngle});
            }
        }
    } catch(e) {
        console.error('Erro ao carregar posição:', e);
    }
}

function initMap() {
    map = L.map('map').setView([towerPos.lat, towerPos.lng], 10);
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: 'OSM', maxZoom: 19
    }).addTo(map);
    
    // Marcador da torre - ARRASTAVEL
    let towerIcon = L.divIcon({
        className: 'tower-icon',
        html: '<div style="width:20px;height:20px;background:#00d4ff;border:3px solid #fff;border-radius:50%;box-shadow:0 0 10px #00d4ff;"></div>',
        iconSize: [20, 20], iconAnchor: [10, 10]
    });
    
    towerMarker = L.marker([towerPos.lat, towerPos.lng], {
        icon: towerIcon,
        draggable: true
    }).addTo(map).bindPopup('Sua Torre - Arraste para mover');
    
    // Evento de arrastar torre
    towerMarker.on('dragend', function(e) {
        let pos = e.target.getLatLng();
        towerPos.lat = pos.lat;
        towerPos.lng = pos.lng;
        document.getElementById('towerCoords').textContent = pos.lat.toFixed(4) + ', ' + pos.lng.toFixed(4);
        if (targetPos) {
            updateAzimuth();
            updateLine();
        }
        // Salvar posicao da torre
        localStorage.setItem('towerLat', pos.lat);
        localStorage.setItem('towerLng', pos.lng);
    });
    
    // Carregar posicao salva
    let savedLat = localStorage.getItem('towerLat');
    let savedLng = localStorage.getItem('towerLng');
    if (savedLat && savedLng) {
        towerPos.lat = parseFloat(savedLat);
        towerPos.lng = parseFloat(savedLng);
        towerMarker.setLatLng([towerPos.lat, towerPos.lng]);
        document.getElementById('towerCoords').textContent = towerPos.lat.toFixed(4) + ', ' + towerPos.lng.toFixed(4);
        map.setView([towerPos.lat, towerPos.lng], 10);
    }
    
    // Clicar no mapa para definir alvo
    map.on('click', function(e) {
        targetPos = {lat: e.latlng.lat, lng: e.latlng.lng};
        document.getElementById('targetCoords').textContent = targetPos.lat.toFixed(4) + ', ' + targetPos.lng.toFixed(4);
        
        if (targetMarker) map.removeLayer(targetMarker);
        let targetIcon = L.divIcon({
            className: 'target-icon',
            html: '<div style="width:16px;height:16px;background:#ef4444;border:2px solid #fff;border-radius:50%;"></div>',
            iconSize: [16, 16], iconAnchor: [8, 8]
        });
        targetMarker = L.marker([targetPos.lat, targetPos.lng], {icon: targetIcon}).addTo(map).bindPopup('Alvo');
        
        updateAzimuth();
        updateLine();
    });
}

function updateAzimuth() {
    let az = calcAzimuth(towerPos.lat, towerPos.lng, targetPos.lat, targetPos.lng);
    let dist = calcDistance(towerPos.lat, towerPos.lng, targetPos.lat, targetPos.lng);
    document.getElementById('azimuth').textContent = az.toFixed(1) + ' graus';
    document.getElementById('distance').textContent = dist.toFixed(1) + ' km';
    
    // az já vem em 0-360° do calcAzimuth
    document.getElementById('targetAngle').value = az.toFixed(1);
    
    // Converter para ±180° antes de enviar ao motor (caminho mais curto)
    let internalAngle = angleToInternal(az);
    
    // Salvar o angulo calculado do mapa
    localStorage.setItem('lastTargetAngle', az.toString());
    
    // Enviar angulo interno (±180°) para o motor
    send({angle: internalAngle});
}

function updateLine() {
    if (line) map.removeLayer(line);
    line = L.polyline([[towerPos.lat, towerPos.lng], [targetPos.lat, targetPos.lng]], {
        color: '#00d4ff', weight: 2, dashArray: '10, 10'
    }).addTo(map);
}

function calcAzimuth(lat1, lng1, lat2, lng2) {
    let dLng = (lng2 - lng1) * Math.PI / 180;
    let lat1r = lat1 * Math.PI / 180;
    let lat2r = lat2 * Math.PI / 180;
    let x = Math.sin(dLng) * Math.cos(lat2r);
    let y = Math.cos(lat1r) * Math.sin(lat2r) - Math.sin(lat1r) * Math.cos(lat2r) * Math.cos(dLng);
    let az = Math.atan2(x, y) * 180 / Math.PI;
    return (az + 360) % 360;
}

function calcDistance(lat1, lng1, lat2, lng2) {
    let R = 6371;
    let dLat = (lat2 - lat1) * Math.PI / 180;
    let dLng = (lng2 - lng1) * Math.PI / 180;
    let a = Math.sin(dLat/2) * Math.sin(dLat/2) + Math.cos(lat1*Math.PI/180) * Math.cos(lat2*Math.PI/180) * Math.sin(dLng/2) * Math.sin(dLng/2);
    return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a));
}

function zoomIn() { map.zoomIn(); }
function zoomOut() { map.zoomOut(); }
function centerMap() { map.setView([towerPos.lat, towerPos.lng], 10); }

window.onload = function() { 
    connect();             // Conexao WebSocket (loadLastPosition sera chamado no onopen)
    initMap();             // Inicializar mapa
    loadLastAngle();       // Carregar ultimo angulo usado (apenas UI)
};
)rawliteral";
