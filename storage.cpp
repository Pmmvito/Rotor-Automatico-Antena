#include "storage.h"
#include "config.h"

StorageManager::StorageManager() {
}

bool StorageManager::begin() {
    // Abre namespace "rotor" em modo leitura/escrita
    bool success = preferences.begin("rotor", false);
    
    #if DEBUG_SERIAL
    if (success) {
        Serial.println("Storage initialized");
    } else {
        Serial.println("Storage initialization failed!");
    }
    #endif
    
    return success;
}

void StorageManager::saveLastPosition(float angle) {
    preferences.putFloat(POSITION_KEY, angle);
    
    #if DEBUG_SERIAL
    Serial.print("Position saved: ");
    Serial.println(angle);
    #endif
}

float StorageManager::loadLastPosition() {
    float angle = preferences.getFloat(POSITION_KEY, 0.0);
    
    #if DEBUG_SERIAL
    Serial.print("Position loaded: ");
    Serial.println(angle);
    #endif
    
    return angle;
}

void StorageManager::saveLastTarget(float angle) {
    preferences.putFloat("lasttarget", angle);
}

float StorageManager::loadLastTarget() {
    return preferences.getFloat("lasttarget", 0.0);
}

void StorageManager::saveAbsolutePosition(float angle) {
    preferences.putFloat("abspos", angle);
    
    #if DEBUG_SERIAL
    Serial.print("Absolute position saved: ");
    Serial.println(angle);
    #endif
}

float StorageManager::loadAbsolutePosition() {
    float angle = preferences.getFloat("abspos", 0.0);
    
    #if DEBUG_SERIAL
    Serial.print("Absolute position loaded: ");
    Serial.println(angle);
    #endif
    
    return angle;
}

void StorageManager::saveCalibrationOffset(float offset) {
    preferences.putFloat(CALIBRATION_KEY, offset);
    
    #if DEBUG_SERIAL
    Serial.print("Calibration offset saved: ");
    Serial.println(offset);
    #endif
}

float StorageManager::loadCalibrationOffset() {
    float offset = preferences.getFloat(CALIBRATION_KEY, 0.0);
    
    #if DEBUG_SERIAL
    Serial.print("Calibration offset loaded: ");
    Serial.println(offset);
    #endif
    
    return offset;
}

bool StorageManager::hasLastPosition() {
    return preferences.isKey(POSITION_KEY);
}

bool StorageManager::hasCalibrationOffset() {
    return preferences.isKey(CALIBRATION_KEY);
}

// ==================================================================================
// SISTEMA DE APRENDIZADO DE INÉRCIA
// ==================================================================================

void StorageManager::saveInertiaFactor(float factor) {
    preferences.putFloat("inertia_f", factor);
    #if DEBUG_SERIAL
    Serial.printf("Inertia factor saved: %.3f\n", factor);
    #endif
}

float StorageManager::loadInertiaFactor() {
    // Default 1.0 = sem compensação. Valores > 1.0 = motor com mais inércia
    return preferences.getFloat("inertia_f", 1.0);
}

void StorageManager::saveBrakingDistance(float distance) {
    preferences.putFloat("brake_dist", distance);
    #if DEBUG_SERIAL
    Serial.printf("Braking distance saved: %.3f deg/(deg/s)\n", distance);
    #endif
}

float StorageManager::loadBrakingDistance() {
    // Default: 0.1 graus de frenagem para cada grau/segundo de velocidade
    return preferences.getFloat("brake_dist", 0.1);
}

void StorageManager::saveOvershootHistory(float avgOvershoot) {
    preferences.putFloat("overshoot", avgOvershoot);
    #if DEBUG_SERIAL
    Serial.printf("Average overshoot saved: %.3f deg\n", avgOvershoot);
    #endif
}

float StorageManager::loadOvershootHistory() {
    return preferences.getFloat("overshoot", 0.0);
}

void StorageManager::saveLearningCycles(int cycles) {
    preferences.putInt("learn_cyc", cycles);
    #if DEBUG_SERIAL
    Serial.printf("Learning cycles saved: %d\n", cycles);
    #endif
}

int StorageManager::loadLearningCycles() {
    return preferences.getInt("learn_cyc", 0);
}

bool StorageManager::hasLearnedParameters() {
    return preferences.isKey("inertia_f") && loadLearningCycles() > 0;
}

void StorageManager::clearAll() {
    preferences.clear();
    
    #if DEBUG_SERIAL
    Serial.println("Storage cleared");
    #endif
}
