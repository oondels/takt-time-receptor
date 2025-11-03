#ifndef SIGNALIZER_H
#define SIGNALIZER_H

#include <Arduino.h>

enum class TipoSinalizador
{
  LED,
  BUZZER
};

enum class EstadoPino : uint8_t
{
  LIGADO = LOW,
  DESLIGADO = HIGH
};


class Sinalizer
{
private:
  TipoSinalizador tipo;
  int pin;
  uint16_t frequencia; // Para buzzer (Hz)
  unsigned long activationDuration;
  unsigned long startTime;
  bool active;
  EstadoPino lastMode;
  String nome;

public:
  Sinalizer(const char* nome, uint8_t pino, TipoSinalizador tipo = TipoSinalizador::LED, uint16_t freq = 0);

  void begin();
  void activate();
  void deactivate();
  bool isActive() const { return active; }
  TipoSinalizador getTipo() const { return tipo; }
  String getNome() const { return nome; }
  unsigned long getActivationDuration() const { return activationDuration; }
};

#endif