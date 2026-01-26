#include "Signalizer.h"
#include <FastLED.h>

CRGB gLeds[NUM_LEDS];

Sinalizer::Sinalizer(const char *nome, uint8_t pino, TipoSinalizador tipo, uint16_t freq)
    : nome(nome), pin(pino), tipo(tipo), frequencia(freq), active(false), startTime(0), lastMode(EstadoPino::DESLIGADO), activationDuration(5000)
{
}

void Sinalizer::begin()
{
  if (this->getTipo() == TipoSinalizador::LED)
  {
    switch (pin)
    {
    case 4:
      FastLED.addLeds<LED_TYPE, 4, COLOR_ORDER>(gLeds, NUM_LEDS).setCorrection(TypicalLEDStrip);
      break;
    default:
      Serial.print("ERRO: GPIO nao suportado para FastLED via template: ");
      Serial.println(pin);
      return;
    }

    FastLED.setBrightness(BRIGHTNESS);
    deactivate();

    Serial.print("Leds configurados no GPIO ");
    Serial.println(pin);
    return;
  }

  pinMode(pin, OUTPUT);
  deactivate();

  Serial.print("Inicializado: ");
  Serial.print(tipo == TipoSinalizador::LED ? "LED" : "BUZZER");
}

void Sinalizer::activate(CRGB color)
{
  if (this->getTipo() == TipoSinalizador::LED)
  {
    fill_solid(gLeds, NUM_LEDS, color);
    FastLED.show();
    active = true;
    startTime = millis();

    Serial.print(nome);
    Serial.println(" ligado");
    return;
  }

  digitalWrite(pin, HIGH);
  active = true;
  startTime = millis();

  Serial.print(nome);
  Serial.println(" ligado");
}

void Sinalizer::deactivate()
{
  if (this->getTipo() == TipoSinalizador::LED)
  {
    fill_solid(gLeds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    active = false;

    Serial.print(nome);
    Serial.println(" desligado");
    return;
  }

  digitalWrite(pin, LOW);

  active = false;

  Serial.print(nome);
  Serial.println(" desligado");
}
