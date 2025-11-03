#include <WiFi.h>
#include <ArduinoJson.h>
#include "wifi/WifiClient.h"
#include "mqtt/MQTTClient.h"
#include "sinalizer/Signalizer.h"
#include "sinalizer/SignalizerController.h"

// Pinos
constexpr int LED1 = 25;
constexpr int LED2 = 26;
constexpr int LED3 = 27;
constexpr int BUZZER = 14;

// Device
const char *DEVICE_ID = "cost-2-2408";

// WiFi & MQTT
const char *SSID = "";
const char *PASSWORD = "";
WifiClient wifiClient(SSID, PASSWORD, 20000); // Timeout de 20 segundos

const char *MQTT_SERVER = "10.110.21.3";
const int MQTT_PORT = 1883;
String MQTT_TOPIC = String("takt/device/") + DEVICE_ID;
const char *MQTT_USER = "dass";
***REMOVED***
MQTTClient mqttClient(MQTT_SERVER, MQTT_PORT, MQTT_USER, MQTT_PASS, MQTT_TOPIC.c_str(), DEVICE_ID);

// Sinalizadores
Sinalizer led1("LED1", LED1, TipoSinalizador::LED);
Sinalizer led2("LED2", LED2, TipoSinalizador::LED);
Sinalizer led3("LED3", LED3, TipoSinalizador::LED);
Sinalizer buzzer("Buzzer", BUZZER, TipoSinalizador::BUZZER, 1000);

SignalizerController sinalizadorController(&led1, &led2, &led3, &buzzer);

// Callback para processar mensagens MQTT com JSON
void onMqttMessage(char *topic, byte *payload, unsigned int length)
{
  Serial.println("\n=== Nova mensagem MQTT ===");

  // O parsing JSON já foi feito automaticamente pela classe MQTTClient
  // Acesse os dados através de mqttClient.comandoRecebido

  Serial.print("Event: ");
  Serial.println(mqttClient.comandoRecebido.event);
  Serial.print("Message: ");
  Serial.println(mqttClient.comandoRecebido.message);
  Serial.print("ID: ");
  Serial.println(mqttClient.comandoRecebido.id);
  Serial.print("Command: ");
  Serial.println(mqttClient.comandoRecebido.takt_count);

  // Marcar que há um novo comando para processar
  mqttClient.newCommand = true;

  Serial.println("==========================\n");
}

void processarComando(int comando)
{
  NivelSinalizacao nivel = static_cast<NivelSinalizacao>(comando);

  switch (nivel)
  {
  case NivelSinalizacao::DESLIGADO:
    sinalizadorController.desligarTodos();
    break;

  case NivelSinalizacao::NIVEL_1:
    sinalizadorController.setNivel(NivelSinalizacao::NIVEL_1);
    break;
  case NivelSinalizacao::NIVEL_2:
    sinalizadorController.setNivel(NivelSinalizacao::NIVEL_2);
    break;
  case NivelSinalizacao::NIVEL_3:
    sinalizadorController.setNivel(NivelSinalizacao::NIVEL_3);
    break;

  default:
    // Comando especial: sequência completa (valor 99 por exemplo)
    if (comando == 99)
    {
      sinalizadorController.sequenciaCompleta();
    }
    else
    {
      Serial.print("Comando inválido: ");
      Serial.println(comando);
    }
    break;
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("\n=== Inicializando Takt Time Receptor ===");

  sinalizadorController.begin();

  Serial.println("Conectando ao WiFi...");
  wifiClient.begin();

  mqttClient.begin();
  mqttClient.setCallback(onMqttMessage);

  Serial.println("Setup concluído!\n");
}

void loop()
{
  // Verificar conexão WiFi
  bool wifiConnected = wifiClient.loop();
  if (!wifiConnected)
  {
    wifiClient.reconnect();
    return;
  }

  // Manter conexão MQTT ativa
  mqttClient.loop();

  if (mqttClient.newCommand)
  {
    mqttClient.newCommand = false;
    int comando = mqttClient.comandoRecebido.takt_count;
    processarComando(comando);
  }

  sinalizadorController.loop();
}
