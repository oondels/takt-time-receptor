
#include "SignalizerController.h"

SignalizerController::SignalizerController(Sinalizer *led1, Sinalizer *led2, Sinalizer *led3, Sinalizer *buzzer)
    : led1(led1), led2(led2), led3(led3), buzzer(buzzer), nivelAtual(NivelSinalizacao::DESLIGADO), lastTaktTime(0)
{
}

void SignalizerController::begin()
{
  led1->begin();
  led2->begin();
  led3->begin();
  buzzer->begin();

  desligarTodos();

  Serial.println("SignalizerController inicializado");
}

bool SignalizerController::validarDependencias(NivelSinalizacao novoNivel)
{
  // LED2 só pode ser ligado se LED1 estiver ligado
  if (novoNivel >= NivelSinalizacao::NIVEL_2 && !led1->isActive())
  {
    Serial.println("AVISO: LED2 requer LED1 ligado.");
    return false;
  }

  // LED3 só pode ser ligado se LED1 e LED2 estiverem ligados
  if (novoNivel >= NivelSinalizacao::NIVEL_3 && (!led1->isActive() || !led2->isActive()))
  {
    Serial.println("AVISO: LED3 requer LED1 e LED2 ligados.");
    return false;
  }

  return true;
}

void SignalizerController::setNivel(NivelSinalizacao nivel)
{
  if (validarDependencias(nivel))
  {
    switch (nivel)
    {
    case NivelSinalizacao::DESLIGADO:
      desligarTodos();
      break;

    case NivelSinalizacao::NIVEL_1:
      led1->activate();
      led2->deactivate();
      led3->deactivate();
      buzzer->deactivate();
      break;

    case NivelSinalizacao::NIVEL_2:
      led1->activate();
      led2->activate();
      led3->deactivate();
      buzzer->deactivate();
      break;

    case NivelSinalizacao::NIVEL_3:
      led1->activate();
      led2->activate();
      led3->activate();
      buzzer->activate();
      lastTaktTime = millis(); // Registrar o tempo de takt time
      break;
    }

    nivelAtual = nivel;
    Serial.print("Nível aplicado: ");
    Serial.println(static_cast<int>(nivel));
    return;
  }
}

void SignalizerController::desligarTodos()
{
  led1->deactivate();
  led2->deactivate();
  led3->deactivate();
  buzzer->deactivate();
  nivelAtual = NivelSinalizacao::DESLIGADO;
  lastTaktTime = 0;

  Serial.println("Todos os dispositivos desligados");
}

void SignalizerController::sequenciaCompleta()
{
  Serial.println("Iniciando sequência completa...");

  desligarTodos();
  delay(500);

  setNivel(NivelSinalizacao::NIVEL_1);
  delay(1000);

  setNivel(NivelSinalizacao::NIVEL_2);
  delay(1000);

  setNivel(NivelSinalizacao::NIVEL_3);
  delay(2000);

  // buzzer->piscar(200, 3);

  Serial.println("Sequência completa finalizada");
}

void SignalizerController::loop()
{
  // Quando o nível 3 estiver atuado, espera o tempo de ativação do buzzer e desativa-o e todos os leds
  if (getNivelAtual() == NivelSinalizacao::NIVEL_3 && buzzer->isActive())
  {
    unsigned long currentTime = millis();
    unsigned long tempoDecorrido = currentTime - lastTaktTime;

    if (tempoDecorrido >= buzzer->getActivationDuration())
    {
      desligarTodos();
      Serial.println("Nível 3 desativado após duração do alarme.");
    }
  }
}