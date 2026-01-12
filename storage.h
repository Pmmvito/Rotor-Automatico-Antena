#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

class StorageManager {
private:
    Preferences preferences;
    
public:
    StorageManager();
    bool begin();
    void saveLastPosition(float angle);
    float loadLastPosition();
    void saveLastTarget(float angle);
    float loadLastTarget();
    void saveAbsolutePosition(float angle);      // Salvar posição absoluta acumulada
    float loadAbsolutePosition();
    void saveCalibrationOffset(float offset);
    float loadCalibrationOffset();
    
    // Sistema de Aprendizado de Inércia
    void saveInertiaFactor(float factor);        // Fator de inércia aprendido (0.5 a 3.0)
    float loadInertiaFactor();
    void saveBrakingDistance(float distance);    // Distância de frenagem em graus por velocidade
    float loadBrakingDistance();
    void saveOvershootHistory(float avgOvershoot); // Média histórica de overshoot
    float loadOvershootHistory();
    void saveLearningCycles(int cycles);         // Quantas vezes o sistema aprendeu
    int loadLearningCycles();
    bool hasLearnedParameters();                 // Verifica se já aprendeu algo
    
    bool hasLastPosition();
    bool hasCalibrationOffset();
    void clearAll();
};

#endif
