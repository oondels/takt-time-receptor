#include "OtaServer.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
#include <WebServer.h>

namespace
{
  constexpr uint16_t OTA_SERVER_PORT = 80;
  constexpr const char *OTA_STATUS_FILE = "/ota/last.json";
  constexpr const char *OTA_STATUS_DIR = "/ota";
  constexpr const char *OTA_HEADER_KEY = "X-OTA-Key";

  WebServer otaServer(OTA_SERVER_PORT);
  DeviceConfig *deviceConfig = nullptr;

  struct OtaRequestState
  {
    bool started = false;
    bool uploadAccepted = false;
    bool updateBegun = false;
    bool updateCompleted = false;
    bool hasError = false;
    int httpCode = 500;
    size_t bytesWritten = 0;
    unsigned long startedAtMs = 0;
    unsigned long finishedAtMs = 0;
    String error;
    String detail;
    String remoteIp;
    int updateErrorCode = 0;
    String updateErrorStr;
  };

  OtaRequestState otaState;

  bool ensureOtaStatusDirectory()
  {
    if (LittleFS.exists(OTA_STATUS_DIR))
    {
      return true;
    }

    return LittleFS.mkdir(OTA_STATUS_DIR);
  }

  void persistOtaStatus(const char *result)
  {
    if (!ensureOtaStatusDirectory())
    {
      Serial.println("Falha ao criar diretório de status OTA");
      return;
    }

    StaticJsonDocument<512> statusDoc;
    statusDoc["started_at_ms"] = otaState.startedAtMs;
    statusDoc["finished_at_ms"] = otaState.finishedAtMs;
    statusDoc["bytes_received"] = otaState.bytesWritten;
    statusDoc["result"] = result;

    if (otaState.updateErrorCode != 0)
    {
      statusDoc["update_error_code"] = otaState.updateErrorCode;
    }

    if (otaState.updateErrorStr.length() > 0)
    {
      statusDoc["update_error_str"] = otaState.updateErrorStr;
    }

    if (otaState.remoteIp.length() > 0)
    {
      statusDoc["remote_ip"] = otaState.remoteIp;
    }

    File statusFile = LittleFS.open(OTA_STATUS_FILE, "w");
    if (!statusFile)
    {
      Serial.println("Falha ao abrir arquivo de status OTA");
      return;
    }

    if (serializeJson(statusDoc, statusFile) == 0)
    {
      Serial.println("Falha ao escrever status OTA");
    }

    statusFile.close();
  }

  void captureRemoteIp()
  {
    String remoteIp = otaServer.client().remoteIP().toString();
    if (remoteIp != "0.0.0.0")
    {
      otaState.remoteIp = remoteIp;
    }
  }

  void abortUpdateIfRunning()
  {
    if (!otaState.updateBegun || otaState.updateCompleted)
    {
      return;
    }

    Update.abort();
    otaState.updateBegun = false;
  }

  void resetOtaState()
  {
    otaState = OtaRequestState{};
  }

  void setOtaError(int code, const String &error, const String &detail)
  {
    if (otaState.hasError)
    {
      return;
    }

    otaState.hasError = true;
    otaState.httpCode = code;
    otaState.error = error;
    otaState.detail = detail;
    otaState.updateErrorCode = Update.getError();
    otaState.updateErrorStr = otaState.updateErrorCode != 0 ? String(Update.errorString()) : detail;
    abortUpdateIfRunning();
  }

  bool isOtaKeyConfigured()
  {
    return deviceConfig != nullptr && deviceConfig->otaKey.length() > 0;
  }

  bool isAuthorizedRequest()
  {
    if (!otaServer.hasHeader(OTA_HEADER_KEY))
    {
      return false;
    }

    return otaServer.header(OTA_HEADER_KEY) == deviceConfig->otaKey;
  }

  bool isMultipartRequest()
  {
    if (!otaServer.hasHeader("Content-Type"))
    {
      return false;
    }

    String contentType = otaServer.header("Content-Type");
    contentType.toLowerCase();
    return contentType.startsWith("multipart/");
  }

  void sendOtaErrorResponse()
  {
    if (!otaState.hasError)
    {
      otaState.httpCode = 500;
      otaState.error = "ota_failed";
      otaState.detail = "unknown error";
    }

    String response =
        String("{\"ok\":false,\"error\":\"") +
        otaState.error +
        "\",\"detail\":\"" +
        otaState.detail +
        "\"}";
    otaServer.send(otaState.httpCode, "application/json", response);
  }

  void beginOtaUpdateRequest()
  {
    resetOtaState();
    otaState.started = true;
    otaState.startedAtMs = millis();
    captureRemoteIp();
    persistOtaStatus("in_progress");

    if (deviceConfig == nullptr)
    {
      setOtaError(500, "server_error", "device config unavailable");
      return;
    }

    if (!isOtaKeyConfigured())
    {
      setOtaError(403, "ota_disabled", "ota_key is empty");
      return;
    }

    if (!isAuthorizedRequest())
    {
      setOtaError(401, "unauthorized", "invalid X-OTA-Key");
      return;
    }

    const bool hasContentLength = otaServer.hasHeader("Content-Length");
    if (hasContentLength)
    {
      int contentLength = otaServer.header("Content-Length").toInt();
      if (contentLength <= 0)
      {
        setOtaError(400, "invalid_content_length", "Content-Length must be greater than zero");
        return;
      }

      if (!Update.begin(static_cast<size_t>(contentLength)))
      {
        setOtaError(500, "update_begin_failed", Update.errorString());
        return;
      }
    }
    else
    {
      if (!Update.begin(UPDATE_SIZE_UNKNOWN))
      {
        setOtaError(500, "update_begin_failed", Update.errorString());
        return;
      }
    }

    otaState.uploadAccepted = true;
    otaState.updateBegun = true;
  }

  void processOtaChunk(uint8_t *buffer, size_t size)
  {
    size_t written = Update.write(buffer, size);
    if (written != size)
    {
      setOtaError(500, "update_write_failed", Update.errorString());
      return;
    }

    otaState.bytesWritten += written;
  }

  void finalizeOtaUpdate()
  {
    if (!Update.end(true))
    {
      setOtaError(500, "update_end_failed", Update.errorString());
      return;
    }

    otaState.updateCompleted = true;
  }

  void handleOtaUpload()
  {
    if (isMultipartRequest())
    {
      HTTPUpload &upload = otaServer.upload();

      if (upload.status == UPLOAD_FILE_START)
      {
        beginOtaUpdateRequest();
        return;
      }

      if (!otaState.uploadAccepted || otaState.hasError)
      {
        return;
      }

      if (upload.status == UPLOAD_FILE_WRITE)
      {
        processOtaChunk(upload.buf, upload.currentSize);
        return;
      }

      if (upload.status == UPLOAD_FILE_END)
      {
        finalizeOtaUpdate();
        return;
      }

      if (upload.status == UPLOAD_FILE_ABORTED)
      {
        setOtaError(400, "upload_aborted", "upload aborted by client");
      }
      return;
    }

    HTTPRaw &raw = otaServer.raw();
    if (raw.status == RAW_START)
    {
      beginOtaUpdateRequest();
      return;
    }

    if (!otaState.uploadAccepted || otaState.hasError)
    {
      return;
    }

    if (raw.status == RAW_WRITE)
    {
      processOtaChunk(raw.buf, raw.currentSize);
      return;
    }

    if (raw.status == RAW_END)
    {
      finalizeOtaUpdate();
      return;
    }

    if (raw.status == RAW_ABORTED)
    {
      setOtaError(400, "upload_aborted", "upload aborted by client");
    }
  }

  void handleOtaPost()
  {
    if (!otaState.started)
    {
      resetOtaState();
      otaState.startedAtMs = millis();
      otaState.finishedAtMs = otaState.startedAtMs;
      captureRemoteIp();
      setOtaError(400, "invalid_request", "no upload payload received");
      persistOtaStatus("fail");
      sendOtaErrorResponse();
      return;
    }

    if (otaState.hasError || !otaState.updateCompleted)
    {
      otaState.finishedAtMs = millis();
      if (otaState.updateErrorStr.length() == 0)
      {
        otaState.updateErrorStr = otaState.detail;
      }
      persistOtaStatus("fail");
      sendOtaErrorResponse();
      resetOtaState();
      return;
    }

    otaState.finishedAtMs = millis();
    otaState.updateErrorCode = 0;
    otaState.updateErrorStr = "";
    persistOtaStatus("ok");

    String response =
        String("{\"ok\":true,\"message\":\"updated\",\"bytes_written\":") +
        String(otaState.bytesWritten) +
        "}";
    otaServer.send(200, "application/json", response);
    delay(200);
    ESP.restart();
  }

  void handleOtaStatus()
  {
    if (!LittleFS.exists(OTA_STATUS_FILE))
    {
      otaServer.send(200, "application/json", "{\"ok\":false,\"message\":\"no status\"}");
      return;
    }

    File statusFile = LittleFS.open(OTA_STATUS_FILE, "r");
    if (!statusFile)
    {
      otaServer.send(500, "application/json", "{\"ok\":false,\"message\":\"status read error\"}");
      return;
    }

    String statusContent = statusFile.readString();
    statusFile.close();

    if (statusContent.length() == 0)
    {
      otaServer.send(200, "application/json", "{\"ok\":false,\"message\":\"no status\"}");
      return;
    }

    otaServer.send(200, "application/json", statusContent);
  }
}

void beginOtaServer(DeviceConfig &cfg)
{
  deviceConfig = &cfg;
  resetOtaState();

  const char *headerKeys[] = {OTA_HEADER_KEY, "Content-Length"};
  otaServer.collectHeaders(headerKeys, 2);

  otaServer.on("/ota", HTTP_POST, handleOtaPost, handleOtaUpload);
  otaServer.on("/ota/status", HTTP_GET, handleOtaStatus);
  otaServer.begin();
}

void loopOtaServer()
{
  otaServer.handleClient();
}
