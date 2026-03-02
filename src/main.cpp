#include <WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "wifi/WifiClient.h"
#include "mqtt/MQTTClient.h"
#include "config/DeviceConfig.h"
#include "config/ConfigStorage.h"
#include "sinalizer/Signalizer.h"
#include "sinalizer/SignalizerController.h"

// Pinos
constexpr int LEDS = 4;
constexpr int BUZZER = 14;

// WiFi & MQTT
const char *SSID = "";
const char *PASSWORD = "";
WifiClient wifiClient(SSID, PASSWORD, 20000); // Timeout de 20 segundos

DeviceConfig deviceConfig = {DEFAULT_DEVICE_ID, DEFAULT_MQTT_USER, DEFAULT_MQTT_PASS, DEFAULT_MQTT_SERVER, DEFAULT_MQTT_PORT};
String MQTT_TOPIC = buildMqttTopic(deviceConfig);
MQTTClient mqttClient(
  deviceConfig.mqttServer.c_str(),
  deviceConfig.mqttPort,
  deviceConfig.mqttUser.c_str(),
  deviceConfig.mqttPass.c_str(),
  MQTT_TOPIC.c_str(),
  deviceConfig.deviceId.c_str());

// Sinalizadores
Sinalizer leds("LEDS", LEDS, TipoSinalizador::LED);
Sinalizer buzzer("Buzzer", BUZZER, TipoSinalizador::BUZZER, 1000);

SignalizerController sinalizadorController(&leds, &buzzer);
bool pendingDeviceIdAck = false;
String pendingOldDeviceId = "";
String pendingNewDeviceId = "";
String pendingRequestId = "";

bool publishDeviceIdUpdateAck()
{
  if (!pendingDeviceIdAck)
  {
    return true;
  }

  if (!mqttClient.isConnected())
  {
    return false;
  }

  String ackTopic = String("takt/device/") + pendingNewDeviceId + "/ack";
  StaticJsonDocument<256> ackDoc;
  ackDoc["event"] = "device_config_ack";
  ackDoc["message"] = "update_device_id_ack";
  ackDoc["status"] = "ok";
  ackDoc["old_device_id"] = pendingOldDeviceId;
  ackDoc["new_device_id"] = pendingNewDeviceId;
  ackDoc["device_id"] = pendingNewDeviceId;
  ackDoc["request_id"] = pendingRequestId;
  ackDoc["timestamp"] = millis();

  String ackPayload;
  serializeJson(ackDoc, ackPayload);

  bool published = mqttClient.publish(ackTopic.c_str(), ackPayload.c_str());
  if (published)
  {
    Serial.print("ACK de update_device_id publicado em: ");
    Serial.println(ackTopic);
    Serial.println(ackPayload);
    pendingDeviceIdAck = false;
  }

  return published;
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

  if (mqttClient.comandoRecebido.event == "device_config" ||
      mqttClient.comandoRecebido.message == "device_config" ||
      mqttClient.comandoRecebido.message == "update_config" ||
      mqttClient.comandoRecebido.message == "update_device_id")
  {
    DeviceConfig newConfig = deviceConfig;
    bool changed = false;
    Serial.println("comando de configuração recebido");

    if (applyConfigFromJson(newConfig, mqttClient.mqttMessage, changed) && changed)
    {
      if (saveConfig(newConfig))
      {
        String oldDeviceId = deviceConfig.deviceId;
        String newDeviceId = newConfig.deviceId;
        String requestId = mqttClient.mqttMessage["request_id"] | "";
        bool isDeviceIdUpdate = mqttClient.comandoRecebido.message == "update_device_id";

        deviceConfig = newConfig;
        MQTT_TOPIC = buildMqttTopic(deviceConfig);
        mqttClient.configure(
            deviceConfig.mqttServer.c_str(),
            deviceConfig.mqttPort,
            deviceConfig.mqttUser.c_str(),
            deviceConfig.mqttPass.c_str(),
            MQTT_TOPIC.c_str(),
            deviceConfig.deviceId.c_str());
        mqttClient.begin();
        mqttClient.disconnect();
        mqttClient.reconnect();

        if (isDeviceIdUpdate)
        {
          pendingOldDeviceId = oldDeviceId;
          pendingNewDeviceId = newDeviceId;
          pendingRequestId = requestId;
          pendingDeviceIdAck = true;

          if (!publishDeviceIdUpdateAck())
          {
            Serial.println("ACK pendente: será reenviado no loop quando reconectar.");
          }
        }

        Serial.println("Configuração atualizada e salva.");
        
        // Aplicar takt_count se presente
        if (mqttClient.mqttMessage.containsKey("takt_count"))
        {
          int taktCommand = deviceConfig.taktCount;
          Serial.print("Aplicando takt_count da configuração: ");
          Serial.println(taktCommand);
          processarComando(taktCommand);
        }
      }
    }

    Serial.println("==========================\n");
    return;
  }

  if (mqttClient.comandoRecebido.message == "test_takt_system") {
    sinalizadorController.sequenciaCompleta();
    return;
  }

  // Marcar que há um novo comando para processar
  mqttClient.newCommand = true;

  Serial.println("==========================\n");
}


void setup()
{
  Serial.begin(115200);
  Serial.println("\n=== Inicializando Takt Time Receptor ===");

  if (!beginConfigStorage(true))
  {
    Serial.println("Falha ao iniciar LittleFS");
  }

  setDefaultConfig(deviceConfig);
  if (loadConfig(deviceConfig))
  {
    Serial.println("Configuração carregada do LittleFS");
  }
  else
  {
    Serial.println("Sem configuração salva, usando defaults");
    saveConfig(deviceConfig);
  }

  MQTT_TOPIC = buildMqttTopic(deviceConfig);
  mqttClient.configure(
      deviceConfig.mqttServer.c_str(),
      deviceConfig.mqttPort,
      deviceConfig.mqttUser.c_str(),
      deviceConfig.mqttPass.c_str(),
      MQTT_TOPIC.c_str(),
      deviceConfig.deviceId.c_str());

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

  if (pendingDeviceIdAck)
  {
    publishDeviceIdUpdateAck();
  }

  if (mqttClient.newCommand)
  {
    mqttClient.newCommand = false;
    int comando = mqttClient.comandoRecebido.takt_count;
    processarComando(comando);
  }

  sinalizadorController.loop();
}
