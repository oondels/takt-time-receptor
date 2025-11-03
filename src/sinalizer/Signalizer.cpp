#include "Signalizer.h"

Sinalizer::Sinalizer(const char *nome, uint8_t pino, TipoSinalizador tipo, uint16_t freq)
    : nome(nome), pin(pino), tipo(tipo), frequencia(freq), active(false), startTime(0), lastMode(EstadoPino::DESLIGADO), activationDuration(5000)
{
}

void Sinalizer::begin()
{
  pinMode(pin, OUTPUT);
  deactivate();

  Serial.print("Inicializado: ");
  Serial.print(tipo == TipoSinalizador::LED ? "LED" : "BUZZER");
}

void Sinalizer::activate()
{
  digitalWrite(pin, LOW);
  active = true;
  startTime = millis();

  Serial.print(nome);
  Serial.println(" ligado");
}

void Sinalizer::deactivate()
{

  digitalWrite(pin, HIGH);

  active = false;

  Serial.print(nome);
  Serial.println(" desligado");
}
