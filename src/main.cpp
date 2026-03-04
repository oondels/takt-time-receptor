#include <WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "wifi/WifiClient.h"
#include "mqtt/MQTTClient.h"
#include "config/DeviceConfig.h"
#include "config/ConfigStorage.h"
#include "sinalizer/Signalizer.h"
#include "sinalizer/SignalizerController.h"
#include "ota/OtaMqttTrigger.h"
#include "ota/OtaServer.h"

// Pinos
constexpr int LEDS = 4;
constexpr int BUZZER = 14;

// WiFi & MQTT
const char *SSID = "Oendels";
const char *PASSWORD = "virx2123";
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
bool otaPending = false;
bool otaInProgress = false;
String pendingUpdateUrl = "";
unsigned long pendingUpdateTimestamp = 0;

bool isValidHttpUrl(const String &url)
{
  return url.length() > 0 &&
         (url.startsWith("http://") || url.startsWith("https://"));
}

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
  if (mqttClient.comandoRecebido.event == "update_takt_time" ||
      mqttClient.comandoRecebido.message == "update_takt_time")
  {
    (void)topic;
    (void)payload;
    (void)length;

    String updateUrl = mqttClient.mqttMessage["update_url"] | "";
    if (!isValidHttpUrl(updateUrl))
    {
      Serial.println("Erro: update_takt_time sem update_url valido. OTA nao agendada.");
      return;
    }

    if (otaInProgress)
    {
      Serial.println("OTA em andamento. Novo update_takt_time ignorado.");
      return;
    }

    if (otaPending && pendingUpdateUrl == updateUrl)
    {
      Serial.println("OTA ja pendente para esta URL. Comando duplicado ignorado.");
      return;
    }

    pendingUpdateUrl = updateUrl;
    pendingUpdateTimestamp = mqttClient.mqttMessage["timestamp"] | millis();
    otaPending = true;

    Serial.println("OTA pendente agendada via MQTT.");
    Serial.print("update_url: ");
    Serial.println(pendingUpdateUrl);
    Serial.print("timestamp: ");
    Serial.println(pendingUpdateTimestamp);
    Serial.println("==========================\n");
    return;
  }

  if (mqttClient.comandoRecebido.event == "device_config" ||
      mqttClient.comandoRecebido.message == "device_config" ||
      mqttClient.comandoRecebido.message == "update_config" ||
      mqttClient.comandoRecebido.message == "update_device_id" ||
      mqttClient.comandoRecebido.message == "update_mqtt")
  {
    (void)topic;
    (void)payload;
    (void)length;

    DeviceConfig previousConfig = deviceConfig;
    DeviceConfig newConfig = deviceConfig;
    bool changed = false;
    bool hasTaktCount = mqttClient.mqttMessage.containsKey("takt_count");
    Serial.println("Comando de configuração recebido");

    if (applyConfigFromJson(newConfig, mqttClient.mqttMessage, changed))
    {
      if (changed)
      {
        if (saveConfig(newConfig))
        {
          deviceConfig = newConfig;

          bool mqttConnectionChanged =
              previousConfig.deviceId != deviceConfig.deviceId ||
              previousConfig.mqttServer != deviceConfig.mqttServer ||
              previousConfig.mqttPort != deviceConfig.mqttPort ||
              previousConfig.mqttUser != deviceConfig.mqttUser ||
              previousConfig.mqttPass != deviceConfig.mqttPass;

          if (mqttConnectionChanged)
          {
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

            if (mqttClient.comandoRecebido.message == "update_device_id")
            {
              String requestId = mqttClient.mqttMessage["request_id"] | "";
              pendingOldDeviceId = previousConfig.deviceId;
              pendingNewDeviceId = deviceConfig.deviceId;
              pendingRequestId = requestId;
              pendingDeviceIdAck = true;

              if (!publishDeviceIdUpdateAck())
              {
                Serial.println("ACK pendente: será reenviado no loop quando reconectar.");
              }
            }
          }

          Serial.println("Configuração atualizada e salva.");
        }
        else
        {
          Serial.println("Falha ao salvar configuração recebida.");
        }
      }

      if (hasTaktCount)
      {
        int taktCommand = mqttClient.mqttMessage["takt_count"] | -1;
        if (taktCommand >= 0 && taktCommand <= 3)
        {
          Serial.print("Aplicando takt_count recebido: ");
          Serial.println(taktCommand);
          processarComando(taktCommand);
        }
        else
        {
          Serial.println("takt_count inválido no payload. Comando ignorado.");
        }
      }
    }

    Serial.println("==========================\n");
    return;
  }

  if (mqttClient.comandoRecebido.message == "test_takt_system")
  {
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
  const String currentFirmwareSignature = String(__DATE__) + " " + String(__TIME__);
  bool shouldResetConfig = hasFirmwareSignatureChanged(currentFirmwareSignature);

  if (shouldResetConfig)
  {
    Serial.println("Nova firmware detectada. Resetando configuracao para defaults.");
    if (!saveConfig(deviceConfig))
    {
      Serial.println("Falha ao salvar defaults apos troca de firmware.");
    }

    if (!saveFirmwareSignature(currentFirmwareSignature))
    {
      Serial.println("Falha ao persistir assinatura da firmware.");
    }
  }
  else if (loadConfig(deviceConfig))
  {
    Serial.println("Configuração carregada do LittleFS");
  }
  else
  {
    Serial.println("Sem configuração salva, usando defaults");
    if (saveConfig(deviceConfig))
    {
      saveFirmwareSignature(currentFirmwareSignature);
    }
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

  beginOtaServer(deviceConfig);

  mqttClient.begin();
  mqttClient.setCallback(onMqttMessage);

  Serial.println("Setup concluído!\n");
}

void loop()
{
  loopOtaServer();

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

  if (otaPending && !otaInProgress)
  {
    otaInProgress = true;
    String updateUrl = pendingUpdateUrl;
    otaPending = false;
    pendingUpdateUrl = "";
    pendingUpdateTimestamp = 0;

    Serial.print("Iniciando OTA agendada via URL: ");
    Serial.println(updateUrl);

    bool otaResult = triggerOtaFromUrl(deviceConfig, updateUrl);
    otaInProgress = false;

    if (otaResult)
    {
      Serial.println("OTA concluida com sucesso. Reiniciando dispositivo.");
      delay(200);
      ESP.restart();
    }
    else
    {
      Serial.println("Falha na OTA agendada. Sistema segue em execucao.");
      Serial.println("Consulte GET /ota/status para detalhes do erro persistido.");
    }
  }

  if (mqttClient.newCommand)
  {
    mqttClient.newCommand = false;
    int comando = mqttClient.comandoRecebido.takt_count;
    processarComando(comando);
  }

  sinalizadorController.loop();
}
