#ifndef SINALIZER_CONTROLLER_H
#define SINALIZER_CONTROLLER_H

#include "Signalizer.h"

enum class NivelSinalizacao : uint8_t
{
    DESLIGADO = 0,
    NIVEL_1 = 1,      // Apenas LED1
    NIVEL_2 = 2,      // LED1 + LED2
    NIVEL_3 = 3       // LED1 + LED2 + LED3 + Buzzer
};

class SignalizerController
{
private:
    Sinalizer* leds;
    Sinalizer* buzzer;
    
    NivelSinalizacao nivelAtual;
    unsigned long lastTaktTime;
    
    bool validarDependencias(NivelSinalizacao novoNivel);

public:
    SignalizerController(Sinalizer* leds, Sinalizer* buzzer);
    
    void begin();
    void loop();
    void setNivel(NivelSinalizacao nivel);
    void desligarTodos();
    void sequenciaCompleta();
    NivelSinalizacao getNivelAtual() const { return nivelAtual; }
};

#endif