#include "ConfigStorage.h"

#include <LittleFS.h>

namespace
{
  constexpr const char *CONFIG_FILE_PATH = "/config.json";
  constexpr const char *FIRMWARE_SIGNATURE_FILE_PATH = "/firmware_signature.txt";
}

bool beginConfigStorage(bool formatOnFail)
{
  return LittleFS.begin(formatOnFail);
}

static void applyField(String &target, const JsonVariantConst &value, bool &changed)
{
  if (!value.isNull())
  {
    String newValue = value.as<String>();
    if (newValue.length() > 0 && newValue != target)
    {
      target = newValue;
      changed = true;
    }
  }
}

bool applyConfigFromJson(DeviceConfig &cfg, const JsonDocument &doc, bool &changed)
{
  changed = false;
  if (doc.containsKey("device_id"))
  {
    applyField(cfg.deviceId, doc["device_id"], changed);
  }
  if (doc.containsKey("mqtt_user"))
  {
    applyField(cfg.mqttUser, doc["mqtt_user"], changed);
  }
  if (doc.containsKey("mqtt_pass"))
  {
    applyField(cfg.mqttPass, doc["mqtt_pass"], changed);
  }
  if (doc.containsKey("mqtt_server"))
  {
    applyField(cfg.mqttServer, doc["mqtt_server"], changed);
  }
  if (doc.containsKey("mqtt_port"))
  {
    int newPort = doc["mqtt_port"].as<int>();
    if (newPort > 0 && newPort != cfg.mqttPort)
    {
      cfg.mqttPort = newPort;
      changed = true;
    }
  }
  if (doc.containsKey("takt_count"))
  {
    int newTaktCount = doc["takt_count"].as<int>();
    if (newTaktCount >= 0 && newTaktCount <= 3)
    {
      if (newTaktCount != cfg.taktCount)
      {
        Serial.println("Atualizando taktCount para: " + String(newTaktCount));
        cfg.taktCount = newTaktCount;
        changed = true;
      }
    }
    else
    {
      Serial.println("takt_count inválido: esperado entre 0 e 3.");
    }
  }
  if (doc.containsKey("ota_key"))
  {
    String newOtaKey = doc["ota_key"].as<String>();
    if (newOtaKey != cfg.otaKey)
    {
      cfg.otaKey = newOtaKey;
      changed = true;
    }
  }

  return true;
}

bool loadConfig(DeviceConfig &cfg)
{
  if (!LittleFS.exists(CONFIG_FILE_PATH))
  {
    return false;
  }

  File file = LittleFS.open(CONFIG_FILE_PATH, "r");
  if (!file)
  {
    return false;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error)
  {
    Serial.print("Erro ao ler config JSON: ");
    Serial.println(error.c_str());
    return false;
  }

  bool changed = false;
  applyConfigFromJson(cfg, doc, changed);
  return true;
}

bool saveConfig(const DeviceConfig &cfg)
{
  File file = LittleFS.open(CONFIG_FILE_PATH, "w");
  if (!file)
  {
    Serial.println("Falha ao abrir config para escrita");
    return false;
  }

  StaticJsonDocument<512> doc;
  doc["device_id"] = cfg.deviceId;
  doc["mqtt_user"] = cfg.mqttUser;
  doc["mqtt_pass"] = cfg.mqttPass;
  doc["mqtt_server"] = cfg.mqttServer;
  doc["mqtt_port"] = cfg.mqttPort;
  doc["takt_count"] = cfg.taktCount;
  doc["ota_key"] = cfg.otaKey;

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("Falha ao salvar config JSON");
    file.close();
    return false;
  }

  file.close();
  return true;
}

bool hasFirmwareSignatureChanged(const String &currentSignature)
{
  File file = LittleFS.open(FIRMWARE_SIGNATURE_FILE_PATH, "r");
  if (!file)
  {
    return true;
  }

  String savedSignature = file.readString();
  file.close();
  savedSignature.trim();

  String normalizedCurrent = currentSignature;
  normalizedCurrent.trim();

  return savedSignature != normalizedCurrent;
}

bool saveFirmwareSignature(const String &signature)
{
  File file = LittleFS.open(FIRMWARE_SIGNATURE_FILE_PATH, "w");
  if (!file)
  {
    Serial.println("Falha ao salvar assinatura de firmware");
    return false;
  }

  size_t written = file.print(signature);
  file.close();

  if (written != signature.length())
  {
    Serial.println("Assinatura de firmware incompleta");
    return false;
  }

  return true;
}
