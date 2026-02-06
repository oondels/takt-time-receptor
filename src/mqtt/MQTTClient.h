#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

struct MensagemComando
{
  String event;
  String message;
  String id;
  float timestamp;
  int takt_count;
};

class MQTTClient
{
  private:
    WiFiClient espClient;
    PubSubClient mqttClient;
    String server;
    int port;
    String user;
    String pass;
    String topic;
    String deviceId;
    unsigned long lastReconnectAttempt = 0;
    unsigned long lastHeartbeat = 0;
    String clientId;
    String statusTopic;
    String heartbeatTopic; 

    void buildTopics();

    void (*messageCallback)(char* topic, byte* payload, unsigned int length);
    static void internalCallback(char* topic, byte* payload, unsigned int length);
    static MQTTClient* instance;

  public:
    MQTTClient(const char* server, int port, const char* user, const char* pass, const char* topic, const char* deviceId);
    void configure(const char* server, int port, const char* user, const char* pass, const char* topic, const char* deviceId);
    void begin();
    void loop();
    void reconnect();
    void disconnect();

    void setCallback(void (*callback)(char* topic, byte*, unsigned int));
    bool isConnected();
    bool publish(const char* topic, const char* payload);
    void publishHeartbeat();
    
    // Métodos para processar JSON
    StaticJsonDocument<512> mqttMessage;
    MensagemComando comandoRecebido;
    bool parseJsonMessage(byte* payload, unsigned int length);
    bool newCommand = false;
};

#endif