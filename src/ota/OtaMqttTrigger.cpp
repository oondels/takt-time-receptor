#include "OtaMqttTrigger.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>

namespace
{
  constexpr const char *OTA_STATUS_DIR = "/ota";
  constexpr const char *OTA_STATUS_FILE = "/ota/last.json";
  constexpr size_t OTA_BUFFER_SIZE = 1024;
  constexpr unsigned long DOWNLOAD_IDLE_TIMEOUT_MS = 15000;

  bool ensureOtaStatusDirectory()
  {
    if (LittleFS.exists(OTA_STATUS_DIR))
    {
      return true;
    }

    return LittleFS.mkdir(OTA_STATUS_DIR);
  }

  void persistOtaStatus(
      unsigned long startedAtMs,
      unsigned long finishedAtMs,
      size_t bytesReceived,
      const char *result,
      const String &error,
      const String &updateUrl)
  {
    if (!ensureOtaStatusDirectory())
    {
      Serial.println("Falha ao criar diretório de status OTA");
      return;
    }

    StaticJsonDocument<512> doc;
    doc["started_at_ms"] = startedAtMs;
    doc["finished_at_ms"] = finishedAtMs;
    doc["bytes_received"] = bytesReceived;
    doc["result"] = result;
    doc["error"] = error;
    doc["update_url"] = updateUrl;

    File statusFile = LittleFS.open(OTA_STATUS_FILE, "w");
    if (!statusFile)
    {
      Serial.println("Falha ao abrir /ota/last.json para escrita");
      return;
    }

    if (serializeJson(doc, statusFile) == 0)
    {
      Serial.println("Falha ao persistir status OTA");
    }

    statusFile.close();
  }

  bool hasValidHttpPrefix(const String &url)
  {
    return url.startsWith("http://") || url.startsWith("https://");
  }
}

bool triggerOtaFromUrl(DeviceConfig &cfg, const String &updateUrl)
{
  (void)cfg;

  unsigned long startedAtMs = millis();
  size_t bytesReceived = 0;
  String error = "";
  bool success = false;
  bool updateBegun = false;

  if (updateUrl.length() == 0 || !hasValidHttpPrefix(updateUrl))
  {
    error = "invalid_update_url";
    persistOtaStatus(startedAtMs, millis(), bytesReceived, "fail", error, updateUrl);
    return false;
  }

  HTTPClient http;
  WiFiClient wifiClient;

  if (!http.begin(wifiClient, updateUrl))
  {
    error = "http_begin_failed";
    persistOtaStatus(startedAtMs, millis(), bytesReceived, "fail", error, updateUrl);
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    error = String("http_status_") + String(httpCode);
    http.end();
    persistOtaStatus(startedAtMs, millis(), bytesReceived, "fail", error, updateUrl);
    return false;
  }

  int contentLength = http.getSize();
  if (contentLength > 0)
  {
    updateBegun = Update.begin(static_cast<size_t>(contentLength));
  }
  else
  {
    updateBegun = Update.begin(UPDATE_SIZE_UNKNOWN);
  }

  if (!updateBegun)
  {
    error = String("update_begin_failed:") + Update.errorString();
    http.end();
    persistOtaStatus(startedAtMs, millis(), bytesReceived, "fail", error, updateUrl);
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[OTA_BUFFER_SIZE];
  unsigned long lastDataAtMs = millis();

  while (http.connected() && (contentLength < 0 || bytesReceived < static_cast<size_t>(contentLength)))
  {
    size_t availableBytes = stream->available();
    if (availableBytes == 0)
    {
      if (millis() - lastDataAtMs > DOWNLOAD_IDLE_TIMEOUT_MS)
      {
        error = "download_timeout";
        break;
      }

      delay(1);
      continue;
    }

    size_t bytesToRead = availableBytes;
    if (bytesToRead > OTA_BUFFER_SIZE)
    {
      bytesToRead = OTA_BUFFER_SIZE;
    }

    int bytesRead = stream->readBytes(buffer, bytesToRead);
    if (bytesRead <= 0)
    {
      error = "stream_read_failed";
      break;
    }

    lastDataAtMs = millis();
    size_t written = Update.write(buffer, static_cast<size_t>(bytesRead));
    if (written != static_cast<size_t>(bytesRead))
    {
      error = String("update_write_failed:") + Update.errorString();
      break;
    }

    bytesReceived += written;
  }

  if (error.length() == 0 && contentLength >= 0 && bytesReceived != static_cast<size_t>(contentLength))
  {
    error = "download_incomplete";
  }

  if (error.length() == 0)
  {
    if (!Update.end(true))
    {
      error = String("update_end_failed:") + Update.errorString();
    }
    else
    {
      success = true;
    }
  }

  if (!success)
  {
    Update.abort();
  }

  http.end();
  persistOtaStatus(
      startedAtMs,
      millis(),
      bytesReceived,
      success ? "ok" : "fail",
      error,
      updateUrl);

  return success;
}
