You are a senior embedded engineer. Work in an ESP32 Arduino project.

Repo evidence (from current code):
- Entry: main.cpp uses WiFi.h + LittleFS + ArduinoJson.
- MQTT: mqtt/MQTTClient.* uses PubSubClient.
- Config: config/ConfigStorage.* persists JSON at "/config.json" on LittleFS.
- DeviceConfig: config/DeviceConfig.* holds device_id, mqtt_* and taktCount.
- There is currently NO HTTP server for OTA.

GOAL
Add a safe OTA update mechanism via HTTP POST (device receives firmware upload and flashes it).
Keep existing MQTT + signalizer behavior intact.

ANTI-HALLUCINATION RULES
- Do not assume framework/paths beyond what you verify in the repository.
- Do exactly ONE task per iteration.
- Before changing anything: list files you will edit and why.
- If build system is unknown (Arduino IDE vs PlatformIO): detect via repo files; do not guess.
- Prefer minimal dependencies. If no async server is present, use ESP32 Arduino built-in WebServer.

DEFINITION OF DONE (overall)
1) Device exposes POST /ota that accepts firmware and flashes using Update API (Arduino).
2) OTA is authenticated (default ON):
   - Add "ota_key" to DeviceConfig + config.json.
   - Require header "X-OTA-Key: <key>" for POST /ota.
3) Persist OTA last status into LittleFS:
   - Create "/ota/last.json" with started_at_ms, finished_at_ms, bytes, result, error, remote_ip if available.
4) Reboot only on success after Update.end(true).
5) Add README instructions with curl examples.

------------------------------------
TASKS (Do them in order, one per iteration)
------------------------------------

TASK 0 — Repo discovery (NO CODE)
- conclude = true
- Confirm server stack availability:
  - Is <WebServer.h> available/used anywhere?
  - Any existing HTTP or AsyncWebServer dependency?
- Confirm build system:
  - platformio.ini? Arduino sketch? CMake?
- Output:
  - “Repo map” with key files and responsibilities (wifi, mqtt, config, sinalizer)
  - Confirm the approach: WebServer + Update.h (or propose alternative ONLY if repo already uses it)

TASK 0 OUTPUT
- Repo map:
  - `src/wifi/WifiClient.*`: conexão Wi-Fi, checagem e reconexão periódica.
  - `src/mqtt/MQTTClient.*`: conexão MQTT (PubSubClient), subscribe/publicação, parse JSON e heartbeat.
  - `src/config/DeviceConfig.*`: modelo de configuração (`device_id`, `mqtt_*`, `taktCount`) e defaults.
  - `src/config/ConfigStorage.*`: persistência em LittleFS (`/config.json`), load/save/apply de JSON.
  - `src/sinalizer/Signalizer.*` e `src/sinalizer/SignalizerController.*`: controle de LEDs/buzzer e níveis de sinalização.
  - `src/main.cpp`: orquestra setup/loop, inicializa LittleFS, Wi-Fi, MQTT e processamento de comandos.
- Stack HTTP detectada:
  - Não há uso de `WebServer.h` no repositório.
  - Não há dependência/uso de `AsyncWebServer` ou `ESPAsyncWebServer`.
- Build system detectado:
  - Projeto PlatformIO com Arduino (`platformio.ini`).
  - Comando padrão de build confirmado: `pio run`.
- Abordagem confirmada para OTA:
  - `WebServer` (core Arduino ESP32) + `Update.h`, conforme instrução de dependências mínimas.

TASK 1 — Add OTA config field (NO OTA flashing yet)
- conclude = true
Files expected:
- config/DeviceConfig.h / .cpp
- config/ConfigStorage.cpp (and headers if needed)

Changes:
- Add DEFAULT_OTA_KEY (empty string is NOT allowed for enabling OTA; if empty, reject requests)
- Extend DeviceConfig with String otaKey;
- Update load/save/applyConfigFromJson to include "ota_key"
- Keep backward compatibility if config.json doesn’t have ota_key

Acceptance:
- Build passes
- Existing config load/save still works

TASK 2 — Add minimal HTTP server module (endpoint skeleton)
- conclude = true
Create new module:
- ota/OtaServer.h
- ota/OtaServer.cpp

Implementation constraints:
- Use WebServer (ESP32 Arduino core) unless repo already uses AsyncWebServer.
- Expose:
  - beginOtaServer(DeviceConfig& cfg)
  - loopOtaServer()
- Add route:
  - POST /ota -> returns 501 Not Implemented (for now)
  - GET /ota/status -> returns last.json if exists, else {ok:false, message:"no status"}

Integrate in main.cpp:
- In setup(): start server after WiFi connect is stable (or start immediately but it will only work after WiFi)
- In loop(): call loopOtaServer() every iteration (non-blocking)

Acceptance:
- POST /ota returns deterministic JSON (not implemented)
- GET /ota/status returns deterministic JSON

TASK 3 — Implement OTA flashing via HTTP POST streaming (Update.h)
- conclude = true
- Add #include <Update.h>
- Support upload method compatible with WebServer:
  - Use server.on("/ota", HTTP_POST, finalHandler, uploadHandler)
  - In uploadHandler: stream each chunk to Update.write
- Validations:
  - Require X-OTA-Key header equals cfg.otaKey
  - Reject if ota_key is empty (secure by default)
  - If Content-Length exists, call Update.begin(contentLength) and validate > 0
  - Abort on any Update error; do not reboot on failure

Responses:
- 200: {ok:true, message:"updated", bytes_written:n} and reboot after short delay
- 4xx/5xx: {ok:false, error:"...", detail:"..."} no reboot

Acceptance:
- Successful upload completes Update.end(true)
- Failure path never reboots

TASK 4 — Persist OTA status to LittleFS (/ota/last.json)
- conclude = false
- Ensure LittleFS is mounted (already done by beginConfigStorage in setup)
- Write last.json on:
  - start (result="in_progress")
  - finish success/failure (result="ok"/"fail", error info)
Fields:
- started_at_ms, finished_at_ms, bytes_received, result
- update_error_code (if available), update_error_str
- remote_ip (if WebServer exposes it; else omit)

Acceptance:
- last.json exists after any attempt
- JSON is valid and not empty

TASK 5 — README + curl examples
- conclude = false
Add documentation:
- How to set ota_key in /config.json (LittleFS)
- Example:
  - curl -X POST http://<ip>/ota \
      -H "X-OTA-Key: <key>" \
      --data-binary "@firmware.bin" \
      -H "Content-Type: application/octet-stream"
- How to check status:
  - curl http://<ip>/ota/status

Acceptance:
- Docs match implementation, including headers and endpoints

------------------------------------
Build/Test protocol per task
------------------------------------
- After each task, run the repo’s standard build command (detect it in TASK 0).
- If no build command exists in repo, add a short “How to build” note to README (do not guess; infer from repo evidence).

START NOW with TASK 0 (Repo discovery). Do NOT write code in TASK 0.
