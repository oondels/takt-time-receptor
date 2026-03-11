# Takt Time Receptor

Firmware para ESP32 que recebe comandos via MQTT e controla sinalização visual com WS2811 e sinalização sonora com buzzer. O projeto também suporta OTA por HTTP direto no dispositivo e OTA pull disparada por MQTT a partir de uma URL de firmware.

## Índice

- [Visão Geral](#visão-geral)
- [Estado Atual do Projeto](#estado-atual-do-projeto)
- [Hardware](#hardware)
- [Configuração](#configuração)
- [Build, Upload e Monitor](#build-upload-e-monitor)
- [Fluxo de Execução](#fluxo-de-execução)
- [MQTT](#mqtt)
- [OTA](#ota)
- [Servidor de Firmware Auxiliar](#servidor-de-firmware-auxiliar)
- [Estrutura do Projeto](#estrutura-do-projeto)
- [Limitações Atuais](#limitações-atuais)
- [Troubleshooting](#troubleshooting)

## Visão Geral

O firmware executa este fluxo:

1. Inicializa Serial, LittleFS, OTA HTTP local e os sinalizadores.
2. Carrega configuração padrão vinda do build.
3. Se detectar um novo firmware, sobrescreve a configuração persistida com os defaults atuais.
4. Caso contrário, tenta carregar `/config.json` do LittleFS.
5. Conecta no Wi-Fi.
6. Conecta no broker MQTT.
7. Escuta mensagens no tópico do dispositivo e processa comandos de sinalização, atualização de configuração e OTA.

## Estado Atual do Projeto

O comportamento atual implementado no código é este:

- Plataforma: ESP32 DevKit v1 com Arduino via PlatformIO.
- Sistema de arquivos: LittleFS.
- Wi-Fi com tentativa inicial de conexão por até 20 segundos.
- Reconexão Wi-Fi a cada 30 segundos quando desconectado.
- Reconexão MQTT a cada 5 segundos quando desconectado.
- Tópico principal de comando: `takt/device/{device_id}`.
- Heartbeat MQTT publicado a cada 30 segundos.
- Tópico de status MQTT publicado com retain.
- Configuração remota por MQTT com persistência em `/config.json`.
- OTA push via `POST /ota` no próprio ESP32.
- OTA pull via mensagem MQTT `update_takt_time` com `update_url`.
- Status da última OTA salvo em `/ota/last.json`.

## Hardware

### Componentes

| Componente | Quantidade | Observação |
|------------|------------|------------|
| ESP32 DevKit v1 | 1 | Board usada no `platformio.ini` |
| Fita/anel WS2811 | 1 | `NUM_LEDS = 10` |
| Buzzer ativo | 1 | Acionado por nível lógico |

### Pinagem atual

```text
GPIO 4  -> Dados do WS2811
GPIO 14 -> Buzzer
GND     -> Terra comum
```

### Observações reais do código

- O suporte FastLED está implementado apenas para LED no GPIO `4`.
- O strip usa `WS2811`, `COLOR_ORDER BRG`, `BRIGHTNESS 255` e `NUM_LEDS 10`.
- O buzzer é tratado como dispositivo digital simples, com acionamento por `LOW`.

## Configuração

### Arquivo `.env`

Crie o arquivo `.env` na raiz a partir do exemplo:

```bash
cp .env.example .env
```

Preencha:

```env
WIFI_SSID=SEU_WIFI
WIFI_PASSWORD=SUA_SENHA
DEFAULT_MQTT_USER=seu_usuario
DEFAULT_MQTT_PASS=sua_senha
DEFAULT_MQTT_SERVER=broker.local
DEFAULT_MQTT_PORT=1883
DEFAULT_OTA_KEY=sua-chave-ota
```

Essas variáveis são lidas por `tools/platformio/load_build_env.py` durante o build e viram defines de compilação usados como valores padrão do firmware.

### Valores padrão do firmware

Os defaults aplicados no boot vêm destes valores:

- `device_id`: `cost-default-id`
- `mqtt_user`: `DEFAULT_MQTT_USER`
- `mqtt_pass`: `DEFAULT_MQTT_PASS`
- `mqtt_server`: `DEFAULT_MQTT_SERVER`
- `mqtt_port`: `DEFAULT_MQTT_PORT`
- `ota_key`: `DEFAULT_OTA_KEY`
- `takt_count`: `0`

### Persistência em LittleFS

O firmware grava configuração em:

- `/config.json`
- `/firmware_signature.txt`
- `/ota/last.json`

Formato atual de `/config.json`:

```json
{
  "device_id": "linha-a-posto-01",
  "mqtt_user": "usuario",
  "mqtt_pass": "senha",
  "mqtt_server": "broker.local",
  "mqtt_port": 1883,
  "takt_count": 0,
  "ota_key": "minha-chave-ota"
}
```

### Reset de configuração em novo firmware

No boot, o firmware compara a assinatura atual baseada em `__DATE__` e `__TIME__` com o valor salvo em `/firmware_signature.txt`.

Se a assinatura mudou:

- a configuração persistida é resetada para os defaults do build atual;
- o novo conteúdo é salvo em `/config.json`;
- a nova assinatura é salva em `/firmware_signature.txt`.

Na prática, uma nova compilação ou uma OTA bem-sucedida sobrescreve a configuração persistida anterior.

## Build, Upload e Monitor

### Requisitos

- PlatformIO instalado
- Python funcional no ambiente do PlatformIO
- ESP32 conectada por USB para upload local

### Dependências do firmware

Definidas em `platformio.ini`:

- `knolleary/PubSubClient@^2.8`
- `bblanchon/ArduinoJson@^6.21.5`
- `fastled/FastLED@^3.10.3`

### Comandos

```bash
# Compilar
pio run

# Upload via USB
pio run --target upload

# Monitor serial
pio device monitor
```

Se quiser acompanhar o serial após o upload, execute os dois comandos separadamente.

### Velocidade do monitor serial

O firmware usa `115200`.

## Fluxo de Execução

### Inicialização

Durante `setup()` o firmware:

1. inicia `Serial`;
2. monta o LittleFS;
3. carrega os valores padrão;
4. avalia troca de firmware e decide entre resetar ou reaproveitar configuração;
5. reconfigura o cliente MQTT com a configuração efetiva;
6. inicializa LEDs e buzzer;
7. inicia conexão Wi-Fi;
8. sobe o servidor OTA HTTP local;
9. inicializa o cliente MQTT e registra callback.

### Loop principal

Durante `loop()` o firmware:

1. atende requisições do servidor OTA local;
2. verifica Wi-Fi e tenta reconexão quando necessário;
3. mantém a conexão MQTT;
4. publica ACK pendente de `update_device_id`, se houver;
5. executa OTA pull pendente, fora do callback MQTT;
6. processa comando novo de sinalização;
7. executa a lógica de auto-desligamento do nível 3.

## MQTT

### Tópicos usados

| Tópico | Direção | Uso |
|--------|---------|-----|
| `takt/device/{device_id}` | subscribe | Comandos do dispositivo |
| `takt/device/{device_id}/status` | publish retained | Online/offline |
| `takt/device/{device_id}/heartbeat` | publish | Telemetria básica |
| `takt/device/{new_device_id}/ack` | publish | ACK de `update_device_id` |

### Payload base aceito

```json
{
  "event": "string",
  "message": "string",
  "id": "string",
  "timestamp": 1730000000,
  "takt_count": 0
}
```

Os campos `event`, `message`, `id`, `timestamp` e `takt_count` são parseados explicitamente. Outros campos continuam disponíveis no JSON bruto armazenado em `mqttMessage`.

### Comandos de sinalização

| `takt_count` | Resultado |
|--------------|-----------|
| `0` | Desliga tudo |
| `1` | LEDs azuis, buzzer desligado |
| `2` | LEDs amarelos, buzzer desligado |
| `3` | LEDs vermelhos, buzzer ligado |
| `99` | Executa `sequenciaCompleta()` |

### Teste funcional

Payload de exemplo:

```json
{
  "event": "takt_alert",
  "message": "test_takt_system",
  "id": "teste-1",
  "timestamp": 1730000000,
  "takt_count": 1
}
```

Quando `message == "test_takt_system"`, o firmware executa a sequência completa e ignora `takt_count`.

### Heartbeat

O heartbeat é publicado a cada 30 segundos com payload semelhante a:

```json
{
  "device_id": "TAKT_DEVICE-cost-default-id-abcdef",
  "timestamp": 123456,
  "uptime": 123,
  "wifi_rssi": -58,
  "free_heap": 214320
}
```

### Atualização remota de configuração

O mesmo tópico principal também aceita atualização de configuração. O firmware aplica apenas os campos presentes no payload.

Payload típico:

```json
{
  "event": "device_config",
  "message": "update_config",
  "device_id": "linha-a-posto-01",
  "mqtt_user": "usuario",
  "mqtt_pass": "senha",
  "mqtt_server": "broker.local",
  "mqtt_port": 1883,
  "ota_key": "minha-chave-ota",
  "takt_count": 2
}
```

Mensagens reconhecidas para esse fluxo:

- `event == "device_config"`
- `message == "device_config"`
- `message == "update_config"`
- `message == "update_device_id"`
- `message == "update_mqtt"`

Comportamento:

- salva a nova configuração em `/config.json`;
- reaplica `device_id`, broker, porta, usuário e senha quando mudam;
- reconecta o MQTT se a conexão precisar ser reconfigurada;
- se a mensagem for `update_device_id`, publica um ACK no tópico do novo dispositivo.

### Exemplos com `mosquitto_pub`

```bash
# Nível 1
mosquitto_pub -h <broker-ip> -t "takt/device/<device_id>" \
  -m '{"event":"takt_ok","message":"normal","id":"1","timestamp":1730000000,"takt_count":1}'

# Nível 2
mosquitto_pub -h <broker-ip> -t "takt/device/<device_id>" \
  -m '{"event":"takt_warning","message":"atencao","id":"2","timestamp":1730000001,"takt_count":2}'

# Nível 3
mosquitto_pub -h <broker-ip> -t "takt/device/<device_id>" \
  -m '{"event":"takt_critical","message":"critico","id":"3","timestamp":1730000002,"takt_count":3}'

# Desligar
mosquitto_pub -h <broker-ip> -t "takt/device/<device_id>" \
  -m '{"event":"reset","message":"off","id":"4","timestamp":1730000003,"takt_count":0}'

# Atualizar configuracao
mosquitto_pub -h <broker-ip> -t "takt/device/<device_id>" \
  -m '{"event":"device_config","message":"update_config","mqtt_server":"broker.local","mqtt_port":1883}'
```

## OTA

O projeto implementa dois fluxos de OTA.

### 1. OTA push: um cliente HTTP envia o firmware para o ESP32

O servidor HTTP roda no próprio ESP32, na porta `80`, com estas rotas:

- `POST /ota`
- `GET /ota/status`

#### Regras

- Header obrigatório: `X-OTA-Key`
- A chave enviada deve ser igual ao `ota_key` configurado no dispositivo
- Tipos aceitos no upload:
  - `application/octet-stream`
  - `multipart/form-data`

#### Exemplo

```bash
curl -X POST "http://<ip-do-esp32>/ota" \
  -H "X-OTA-Key: <ota-key>" \
  -H "Content-Type: application/octet-stream" \
  --data-binary "@.pio/build/esp32doit-devkit-v1/firmware.bin"
```

Resposta de sucesso:

```json
{"ok":true,"message":"updated","bytes_written":123456}
```

Em caso de sucesso, o dispositivo reinicia logo após responder.

#### Consultar status

```bash
curl "http://<ip-do-esp32>/ota/status"
```

Resposta típica:

```json
{
  "started_at_ms": 10482,
  "finished_at_ms": 18337,
  "bytes_received": 877733,
  "result": "ok",
  "remote_ip": "192.168.1.50"
}
```

Se houver falha, o JSON pode incluir:

- `update_error_code`
- `update_error_str`

### 2. OTA pull: MQTT agenda o download do firmware por URL

O firmware aceita o disparo quando:

- `event == "update_takt_time"`, ou
- `message == "update_takt_time"`

Campos relevantes:

- `update_url`: obrigatório
- `timestamp`: opcional

Exemplo:

```json
{
  "event": "update_takt_time",
  "update_url": "http://firmware-host.local:9923/update-takttime",
  "timestamp": 1730000000
}
```

Comportamento real:

- o callback MQTT não executa a OTA;
- o callback apenas agenda a operação;
- a execução ocorre no `loop()` principal;
- se a OTA terminar com sucesso, o ESP32 reinicia;
- se falhar, o firmware continua rodando.

Status salvo em `/ota/last.json` inclui:

- `started_at_ms`
- `finished_at_ms`
- `bytes_received`
- `result`
- `error`
- `update_url`

#### Exemplo de trigger MQTT

```bash
mosquitto_pub -h <broker-ip> -t "takt/device/<device_id>" \
  -m '{"event":"update_takt_time","update_url":"http://<host-ip>:9923/update-takttime","timestamp":1730000000}'
```

## Servidor de Firmware Auxiliar

Existe um servidor Node.js simples em `tools/ota-server` para servir o binário de firmware.

### Endpoints

| Método | Rota | Comportamento |
|--------|------|---------------|
| `GET` | `/` | Health check simples |
| `GET` | `/update-takttime` | Retorna o firmware |

### Variáveis de ambiente

| Variável | Default | Uso |
|----------|---------|-----|
| `PORT` | `9923` | Porta HTTP |
| `FIRMWARE_PATH` | `./firmware.bin` | Caminho do binário |

### Executar

Instale as dependências dentro de `tools/ota-server`:

```bash
cd tools/ota-server
npm install
PORT=9923 FIRMWARE_PATH="../../.pio/build/esp32doit-devkit-v1/firmware.bin" npm start
```

### Testar

```bash
curl http://localhost:9923/
curl -o firmware.bin http://localhost:9923/update-takttime
```

## Estrutura do Projeto

```text
takt-time-receptor/
├── .env.example
├── platformio.ini
├── src/
│   ├── main.cpp
│   ├── config/
│   │   ├── ConfigStorage.cpp
│   │   ├── ConfigStorage.h
│   │   ├── DeviceConfig.cpp
│   │   └── DeviceConfig.h
│   ├── mqtt/
│   │   ├── MQTTClient.cpp
│   │   └── MQTTClient.h
│   ├── ota/
│   │   ├── OtaMqttTrigger.cpp
│   │   ├── OtaMqttTrigger.h
│   │   ├── OtaServer.cpp
│   │   └── OtaServer.h
│   ├── sinalizer/
│   │   ├── Signalizer.cpp
│   │   ├── Signalizer.h
│   │   ├── SignalizerController.cpp
│   │   └── SignalizerController.h
│   └── wifi/
│       ├── WifiClient.cpp
│       └── WifiClient.h
├── include/
│   └── BuildEnv.h
├── tools/
│   ├── ota-server/
│   │   ├── package.json
│   │   ├── package-lock.json
│   │   └── server.js
│   └── platformio/
│       └── load_build_env.py
└── docs/
```

## Limitações Atuais

- O suporte FastLED está amarrado ao GPIO `4`.
- A validação de dependências do `SignalizerController` só checa ponteiros nulos.
- Não existe regra de progressão obrigatória entre níveis 1, 2 e 3.
- O cliente MQTT usa `WiFiClient`, sem TLS.
- O servidor OTA local escuta em HTTP simples, porta `80`.
- A configuração persistida é resetada quando o firmware muda.
- O `package.json` da raiz não controla o firmware; o uso prático de Node está em `tools/ota-server`.

## Troubleshooting

### Wi-Fi não conecta

1. Confirme SSID e senha no `.env`.
2. Verifique se a rede é 2.4 GHz.
3. Observe o monitor serial em `115200`.
4. Lembre que a reconexão automática ocorre a cada 30 segundos.

### MQTT não conecta

1. Verifique host, porta, usuário e senha.
2. Confirme se o broker está acessível pela rede do ESP32.
3. Verifique se o `device_id` usado no tópico é o esperado.
4. Lembre que a reconexão MQTT tenta novamente a cada 5 segundos.

### OTA push falha

1. Verifique se `ota_key` não está vazia.
2. Confirme o header `X-OTA-Key`.
3. Confirme que o binário enviado é o `firmware.bin` correto.
4. Consulte `GET /ota/status` depois da tentativa.

### OTA pull falha

1. Verifique se `update_url` começa com `http://` ou `https://`.
2. Confirme que o servidor de firmware responde `200 OK`.
3. Verifique o conteúdo de `/ota/last.json` via endpoint `/ota/status` quando aplicável.
4. Confirme que o ESP32 tem acesso HTTP à URL publicada.

### LEDs ou buzzer não respondem

1. Confirme a pinagem real do hardware.
2. Verifique GND comum.
3. Lembre que apenas o GPIO `4` está implementado para o WS2811.
4. No nível `3`, o buzzer e os LEDs desligam automaticamente após 5 segundos.
