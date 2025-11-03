#include "MQTTClient.h"

// Inicialização do ponteiro estático
MQTTClient *MQTTClient::instance = nullptr;

MQTTClient::MQTTClient(const char *server, int port, const char *user, const char *pass, const char *topic, const char *deviceId)
    : mqttClient(espClient)
{
  SERVER = server;
  PORT = port;
  USER = user;
  PASS = pass;
  TOPIC = topic;
  clientId = deviceId;

  // tópicos de status e heartbeat
  statusTopic = String("takt/device/") + clientId + "/status";
  heartbeatTopic = String("takt/device/") + clientId + "/heartbeat";

  // Configurar instância estática para callback
  instance = this;

  // Inicializar estrutura de comando
  comandoRecebido = {"", "", "", 0.0f, 0};
}

void MQTTClient::begin()
{
  mqttClient.setClient(espClient);
  mqttClient.setServer(SERVER, PORT);
  clientId = "TAKT_DEVICE-" + clientId + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  mqttClient.setBufferSize(512);
}

void MQTTClient::setCallback(void (*callback)(char *, byte *, unsigned int))
{
  messageCallback = callback;
  // Usar o internalCallback para processar JSON automaticamente
  mqttClient.setCallback(internalCallback);
}

// Callback interno para processar JSON automaticamente
void MQTTClient::internalCallback(char *topic, byte *payload, unsigned int length)
{
  if (instance != nullptr)
  {
    // Parse automático do JSON
    if (instance->parseJsonMessage(payload, length))
    {
      Serial.println("JSON parseado com sucesso!");
    }
    else
    {
      Serial.println("Erro ao parsear JSON!");
    }

    // Chamar callback do usuário se configurado
    if (instance->messageCallback != nullptr)
    {
      instance->messageCallback(topic, payload, length);
    }
  }
}

bool MQTTClient::parseJsonMessage(byte *payload, unsigned int length)
{
  mqttMessage.clear();

  DeserializationError error = deserializeJson(mqttMessage, payload, length);
  if (error)
  {
    Serial.print("Erro ao parsear JSON: ");
    Serial.println(error.c_str());
    return false;
  }

  // Extrair campos do JSON para a estrutura
  comandoRecebido.event = mqttMessage["event"] | "";
  comandoRecebido.message = mqttMessage["message"] | "";
  comandoRecebido.id = mqttMessage["id"] | "";
  comandoRecebido.timestamp = mqttMessage["timestamp"] | 0.0f;
  comandoRecebido.takt_count = mqttMessage["takt_count"] | 0;

  Serial.println("=== Mens5agem MQTT recebida ===");
  Serial.print("Event: ");
  Serial.println(comandoRecebido.event);
  Serial.print("Message: ");
  Serial.println(comandoRecebido.message);
  Serial.print("ID: ");
  Serial.println(comandoRecebido.id);
  Serial.print("Timestamp: ");
  Serial.println(comandoRecebido.timestamp);
  Serial.print("Command: ");
  Serial.println(comandoRecebido.takt_count);
  Serial.println("=============================");

  return true;
}

bool MQTTClient::isConnected()
{
  return mqttClient.connected();
}

void MQTTClient::reconnect()
{
  if (!mqttClient.connected())
  {
    Serial.print("Reconectando ao broker MQTT...");

    // Last Will and Testament (LWT)
    // Formato: connect(clientId, user, pass, willTopic, willQoS, willRetain, willMessage)
    bool connected = mqttClient.connect(
        clientId.c_str(),
        USER,
        PASS,
        statusTopic.c_str(), // Will Topic
        1,                   // Will QoS
        true,                // Will Retain
        "offline"            // Will Message
    );

    if (connected)
    {
      Serial.println(" Conectado!");

      // Publicar status online (retained)
      mqttClient.publish(statusTopic.c_str(), "online", true);

      // Inscrever no tópico de comandos
      mqttClient.subscribe(TOPIC);
      Serial.print("Inscrito no tópico: ");
      Serial.println(TOPIC);

      publishHeartbeat();
      lastReconnectAttempt = 0;
    }
    else
    {
      Serial.print(" Falha! rc=");
      Serial.print(mqttClient.state());
      Serial.println(" - tentando novamente em 5s");
    }
  }
}

void MQTTClient::publishHeartbeat()
{
  if (mqttClient.connected())
  {
    // JSON com informações do dispositivo
    StaticJsonDocument<256> doc;
    doc["device_id"] = clientId;
    doc["timestamp"] = millis();
    doc["uptime"] = millis() / 1000;
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["free_heap"] = ESP.getFreeHeap();

    String payload;
    serializeJson(doc, payload);

    mqttClient.publish(heartbeatTopic.c_str(), payload.c_str(), false);

    Serial.print("Heartbeat enviado: ");
    Serial.println(payload);
  }
}

void MQTTClient::loop()
{
  if (!mqttClient.connected())
  {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000)
    {
      lastReconnectAttempt = now;
      reconnect();
    }
  }
  else
  {
    mqttClient.loop();

    unsigned long now = millis();
    if (now - lastHeartbeat > 30000)
    {
      lastHeartbeat = now;
      publishHeartbeat();
    }
  }
}

bool MQTTClient::publish(const char *topic, const char *payload)
{
  if (!mqttClient.connected())
  {
    Serial.println("MQTT não conectado - não é possível publicar");
    return false;
  }

  bool result = mqttClient.publish(topic, payload);

  if (result)
  {
    Serial.print("Publicado em [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(payload);
  }
  else
  {
    Serial.println("Falha ao publicar mensagem");
  }

  return result;
}