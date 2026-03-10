# Takt Time Receptor

Sistema embarcado ESP32 para monitoramento visual e sonoro de takt time em ambientes industriais, com comunicação MQTT e controle progressivo de sinalização.

## Índice

- [Visão Geral](#visão-geral)
- [Características](#características)
- [Arquitetura](#arquitetura)
- [Hardware](#hardware)
- [Configuração](#configuração)
- [Protocolo MQTT](#protocolo-mqtt)
- [Níveis de Sinalização](#níveis-de-sinalização)
- [Estrutura do Projeto](#estrutura-do-projeto)
- [Compilação e Upload](#compilação-e-upload)
- [OTA via HTTP](#ota-via-http)
- [Servidor de Firmware (Node.js)](#servidor-de-firmware-nodejs)
- [Uso](#uso)

## Visão Geral

O **Takt Time Receptor** é um dispositivo IoT baseado em ESP32 que recebe comandos via MQTT para controlar um sistema de sinalização progressiva composto por uma fita/anel WS2811 e 1 buzzer. O sistema foi projetado para alertar operadores sobre o status do takt time em processos produtivos.

### Funcionamento

1. Conecta-se à rede WiFi configurada
2. Estabelece conexão com broker MQTT
3. Escuta comandos no tópico `takt/device/{DEVICE_ID}`
4. Processa comandos JSON e ativa níveis de sinalização
5. Detecta `update_takt_time` e agenda OTA sem bloquear o callback MQTT
6. Executa OTA no `loop()` principal e reinicia apenas em caso de sucesso

## Características

- **Conexão WiFi**: Auto-reconexão automática em caso de perda de sinal
- **Comunicação MQTT**: Processamento automático de mensagens JSON
- **OTA assíncrona via MQTT**: Trigger por `update_takt_time` com execução fora do callback
- **Cliente OTA HTTP**: Download de firmware por URL (`http://` ou `https://`) com streaming
- **Persistência de Status OTA**: Resultado salvo em `/ota/last.json`
- **Sinalização Progressiva**: 4 comandos de sinalização (`0`, `1`, `2`, `3` e teste `99`)
- **Validação Básica**: Verificação de dependências internas do controlador (ponteiros válidos)
- **Auto-desligamento**: Timer configurável para nível crítico
- **Arquitetura Modular**: Separação de responsabilidades (SOLID)
- **Logs Detalhados**: Monitoramento via Serial (115200 baud)

## Arquitetura

### Diagrama de Componentes

```
┌─────────────────────────────────────────────────────────┐
│                        main.cpp                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ WifiClient   │  │ MQTTClient   │  │ Signalizer   │  │
│  │              │  │              │  │ Controller   │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│         │                 │                  │          │
│         ▼                 ▼                  ▼          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   ESP32      │  │   Broker     │  │ WS2811 +     │  │
│  │   WiFi       │  │   MQTT       │  │   Buzzer     │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### Componentes Principais

#### 1. **WifiClient**
- Gerencia conexão WiFi
- Implementa reconnect automático
- Timeout configurável (20 segundos padrão)

#### 2. **MQTTClient**
- Gerencia conexão com broker MQTT
- Parse automático de mensagens JSON
- Estrutura `MensagemComando` para dados tipados
- Reconnect com intervalo de 5 segundos

#### 3. **Sinalizer**
- Controla dispositivos individuais (LED ou Buzzer)
- Suporta fita/anel WS2811 via FastLED
- Timer interno para controle de duração
- Métodos: `activate()`, `deactivate()`, `isActive()`

#### 4. **SignalizerController**
- Orquestra múltiplos Sinalizers
- Implementa lógica de níveis progressivos
- Valida dependências internas básicas do controlador
- Auto-desligamento após duração configurável

## 🔧 Hardware

### Componentes Necessários

| Componente | Quantidade | Especificação |
|------------|------------|---------------|
| ESP32 DevKit v1 | 1 | Qualquer variante |
| Fita/anel WS2811 | 1 | Configurado para 10 LEDs |
| Buzzer Ativo | 1 | 5V, 1000Hz |
| Protoboard | 1 | Opcional |
| Jumpers | - | Conforme necessário |

### Pinagem

```cpp
GPIO 4  → Dados WS2811 (níveis via cores)
GPIO 14 → Buzzer (Alarme)
GND     → Comum
```

### Diagrama de Conexão

```
ESP32                    Componentes
─────                    ───────────
GPIO 4  ──────────────── DIN WS2811
GPIO 14 ──────────────── Buzzer (+)
GND     ──────────────── GND comum
```

> **Nota**: A sinalização visual usa `FastLED` (WS2811) com 10 LEDs (`NUM_LEDS = 10`).

## Configuração

### 1. Credenciais WiFi

Crie o arquivo `.env` na raiz do projeto (o arquivo real é ignorado pelo Git):

```bash
cp .env.example .env
```

### 2. Configuração MQTT

Preencha as variáveis abaixo no `.env`:

```env
WIFI_SSID=SUA_REDE_WIFI
WIFI_PASSWORD=SUA_SENHA
DEFAULT_MQTT_USER=usuario
DEFAULT_MQTT_PASS=senha
DEFAULT_MQTT_SERVER=IP_DO_BROKER
DEFAULT_MQTT_PORT=1883
DEFAULT_OTA_KEY=sua-chave-ota
```

Esses valores são carregados no build (`pio run`) via `extra_scripts` do PlatformIO e viram defaults de firmware.

### 3. Persistência e Config Remota

- As configurações de dispositivo são salvas em **LittleFS** (arquivo `/config.json`).
- No boot, o dispositivo carrega o arquivo e aplica as configurações.
- Atualizações podem ser feitas via MQTT, salvando automaticamente no LittleFS.
- O `DEVICE_ID` padrão atual é **cost-default-id**.
- Para habilitar OTA HTTP, configure `ota_key` com valor não vazio.
- Em firmware nova (build novo ou OTA), a configuração persistida é resetada para defaults.

Exemplo de `/config.json`:

```json
{
  "device_id": "cost-3-3608",
  "mqtt_user": "usuario",
  "mqtt_pass": "senha",
  "mqtt_server": "10.110.193.135",
  "mqtt_port": 1883,
  "takt_count": 0,
  "ota_key": "minha-chave-ota"
}
```

### 4. Duração do Alarme

```cpp
// No código atual, a frequência do buzzer é passada no construtor
Sinalizer buzzer("Buzzer", BUZZER, TipoSinalizador::BUZZER, 1000);
//                                                           freq
```

> **Observação**: no firmware atual, `activationDuration` é fixado internamente em `5000ms`.

## Protocolo MQTT

### Tópico de Subscrição

```
takt/device/{DEVICE_ID}
ex: takt/device/cost-3-3508
```

### Formato da Mensagem (JSON)

```json
{
  "event": "string",
  "message": "string",
  "id": "string",
  "timestamp": 1234567890.0,
  "takt_count": 0
}
```

Exemplo de teste de funcionalidade
```json
{
  "event": "takt_alert",
  "message": "test_takt_system",
  "timestamp": 1698412800.5,
  "takt_count": 1
}
```

> **Observação**: quando `message == "test_takt_system"`, o firmware executa `sequenciaCompleta()` e ignora o `takt_count` do payload.

### Campos

| Campo | Tipo | Descrição |
|-------|------|-----------|
| `event` | String | Tipo de evento (ex: "takt_alert") |
| `message` | String | Mensagem descritiva |
| `id` | String | Identificador único da mensagem |
| `timestamp` | Float | Timestamp Unix |
| `takt_count` | Integer | Comando de sinalização (0-3, 99) |

### Atualização remota de configuração

#### Tópico recomendado

```
takt/device/{DEVICE_ID}
```

#### JSON esperado

```json
{
  "event": "device_config",
  "message": "update_config",
  "device_id": "cost-3-3608",
  "mqtt_user": "usuario",
  "mqtt_pass": "senha",
  "mqtt_server": "10.110.193.135",
  "mqtt_port": 1883
}
```

#### Observações

- O dispositivo aplica apenas os campos presentes.
- Após salvar, o cliente MQTT é reconfigurado e reconecta automaticamente.

### Exemplo de Mensagem

```json
{
  "event": "takt_warning",
  "message": "Produção atrasada",
  "id": "msg-12345",
  "timestamp": 1698765432.5,
  "takt_count": 2
}
```

### Trigger OTA remota (`update_takt_time`)

Mensagem de OTA aceita quando:
- `event == "update_takt_time"` **ou**
- `message == "update_takt_time"`

Campos esperados:
- `update_url` (obrigatório): deve iniciar com `http://` ou `https://`
- `timestamp` (opcional)

Exemplo:

```json
{
  "event": "update_takt_time",
  "update_url": "http://192.168.1.20:2399/update-takttime",
  "timestamp": 1730000000
}
```

Comportamento:
- O callback MQTT apenas agenda a OTA (`otaPending`)
- A execução ocorre no `loop()` principal (`single-flight`, sem disparos simultâneos)
- Em sucesso: grava status e reinicia o ESP32
- Em falha: grava status `fail` e mantém o sistema em execução

## Níveis de Sinalização

### Tabela de Comandos

| Comando (`takt_count`) | Nível | Cor WS2811 | Buzzer | Descrição |
|---------|-------|-----------|--------|-----------|
| `0` | DESLIGADO | Off | Off | Tudo desligado |
| `1` | NÍVEL_1 | Azul | Off | Alerta inicial |
| `2` | NÍVEL_2 | Amarelo | Off | Atenção aumentada |
| `3` | NÍVEL_3 | Vermelho | On | **Crítico + Alarme** |
| `99` | SEQUÊNCIA | Cores em sequência | Sequência | Teste completo do sistema |

### Regras de Validação

#### Regras de Aplicação

1. Nível `1`: liga LEDs WS2811 em azul e desliga buzzer.
2. Nível `2`: liga LEDs WS2811 em amarelo e desliga buzzer.
3. Nível `3`: liga LEDs WS2811 em vermelho e liga buzzer.

#### Validação implementada atualmente

- O controlador valida apenas se os ponteiros de LEDs e buzzer foram inicializados.
- Não existe, no código atual, uma regra de progressão obrigatória entre os níveis `1`, `2` e `3`.

#### Auto-desligamento

- No **Nível 3**, após `activationDuration` (padrão: 5000ms), o sistema **desliga automaticamente** todos os dispositivos
- O timer é resetado a cada nova ativação do Nível 3

### Fluxo de Ativação

```
Comando 1 → LEDs azul, buzzer off
            ↓
Comando 2 → LEDs amarelo, buzzer off
            ↓
Comando 3 → LEDs vermelho, buzzer on
            ↓ (após activationDuration)
           Tudo OFF (automático)
```

## Estrutura do Projeto

```
takt-time-receptor/
├── src/
│   ├── main.cpp                    # Orquestração principal
│   ├── wifi/
│   │   ├── WifiClient.h            # Header WiFi
│   │   └── WifiClient.cpp          # Implementação WiFi
│   ├── mqtt/
│   │   ├── MQTTClient.h            # Header MQTT
│   │   └── MQTTClient.cpp          # Implementação MQTT
│   ├── config/
│   │   ├── DeviceConfig.h          # Modelo e defaults de configuração
│   │   ├── DeviceConfig.cpp        # Inicialização de defaults e tópicos
│   │   ├── ConfigStorage.h         # Persistência LittleFS (config e assinatura firmware)
│   │   └── ConfigStorage.cpp       # Load/save de /config.json
│   ├── ota/
│   │   ├── OtaServer.h             # Endpoints HTTP de OTA
│   │   ├── OtaServer.cpp           # Upload OTA e status
│   │   ├── OtaMqttTrigger.h        # Trigger OTA por URL recebida via MQTT
│   │   └── OtaMqttTrigger.cpp      # Cliente HTTP OTA + persistência /ota/last.json
│   └── sinalizer/
│       ├── Signalizer.h            # Header dispositivo individual
│       ├── Signalizer.cpp          # Implementação dispositivo
│       ├── SignalizerController.h  # Header controlador
│       └── SignalizerController.cpp # Implementação controlador
├── tools/
│   └── ota-server/
│       ├── server.js               # Servidor firmware.bin (GET /update-takttime)
│       └── package.json
├── platformio.ini                  # Configuração PlatformIO
└── README.md                       # Este arquivo
```

## Compilação e Upload

### Requisitos

- [PlatformIO](https://platformio.org/) instalado
- VS Code com extensão PlatformIO (recomendado)

### Dependências (platformio.ini)

```ini
[env:esp32doit-devkit-v1]
platform = espressif32
board = esp32doit-devkit-v1
framework = arduino
board_build.filesystem = littlefs
lib_deps = 
    knolleary/PubSubClient@^2.8
    bblanchon/ArduinoJson @ ^6.21.5
    fastled/FastLED@^3.10.3
monitor_speed = 115200
```

## OTA via HTTP

### Fluxos OTA (Push vs Pull)

```text
                     ┌──────────────────────────────┐
                     │        Broker MQTT           │
                     └──────────────┬───────────────┘
                                    │ update_takt_time + update_url
                                    ▼
┌──────────────────────┐   agenda OTA   ┌──────────────────────────┐
│      ESP32 (MQTT)    ├───────────────►│ loop() chama OTA client  │
└──────────────────────┘                └──────────────┬───────────┘
                                                       │ HTTP GET update_url
                                                       ▼
                                            ┌──────────────────────────┐
                                            │ Servidor firmware.bin    │
                                            │ (tools/ota-server)       │
                                            └──────────────────────────┘

Fluxo Push:
Cliente HTTP ──POST /ota (+ X-OTA-Key)──► ESP32 (OtaServer)
```

### OTA Push (HTTP direto para ESP32)

#### Upload de firmware

Endpoint: `POST /ota`  
Header obrigatório: `X-OTA-Key: <ota_key>`
Tipos aceitos: `application/octet-stream` (raw) e `multipart/form-data`.

```bash
curl -X POST "http://<ip-do-esp32>/ota" \
  -H "X-OTA-Key: <key>" \
  -H "Content-Type: application/octet-stream" \
  --data-binary "@.pio/build/esp32doit-devkit-v1/firmware.bin"
```

Resposta de sucesso esperada:

```json
{"ok":true,"message":"updated","bytes_written":123456}
```

#### Consultar status da última tentativa

Endpoint: `GET /ota/status`

```bash
curl "http://<ip-do-esp32>/ota/status"
```

Exemplo de retorno:

```json
{
  "started_at_ms": 10482,
  "finished_at_ms": 18337,
  "bytes_received": 877733,
  "result": "ok",
  "remote_ip": "10.100.1.80"
}
```

Em falhas, o JSON também pode conter `update_error_code` e `update_error_str`.

### OTA Pull (MQTT + URL de firmware)

Trigger MQTT aceito:
- `event == "update_takt_time"` ou `message == "update_takt_time"`
- `update_url` obrigatório (`http://` ou `https://`)

Fluxo de execução:
- Callback MQTT apenas agenda (`otaPending`)
- Execução ocorre no `loop()` (`triggerOtaFromUrl`)
- Sem execução concorrente (`single-flight`)
- Reinício apenas em sucesso

Quando a OTA é disparada por MQTT, o status em `/ota/last.json` inclui:
- `error`
- `update_url`

### Comandos

```bash
# Compilar
pio run

# Upload para ESP32
pio run --target upload

# Monitor Serial
pio device monitor

# Compilar + Upload + Monitor
pio run --target upload && pio device monitor
```

### Trigger OTA Pull via MQTT (exemplo)

```bash
mosquitto_pub -h <broker-ip> -t "takt/device/<DEVICE_ID>" \
  -m '{"event":"update_takt_time","update_url":"http://<host-ip>:9923/update-takttime","timestamp":1730000000}'
```

## Servidor de Firmware (Node.js)

Servidor minimalista em `tools/ota-server` para entregar `firmware.bin`.

Comportamento:
- `GET /update-takttime`
- `Content-Type: application/octet-stream`
- `Content-Length` calculado por `fs.stat`
- `PORT` via `process.env.PORT` (default `9923`)
- `FIRMWARE_PATH` via `process.env.FIRMWARE_PATH` (default `./firmware.bin`)

### Executar

```bash
cd tools/ota-server
PORT=9923 FIRMWARE_PATH="../../.pio/build/esp32doit-devkit-v1/firmware.bin" node server.js
```

### Testar download

```bash
curl -v http://localhost:9923/update-takttime -o firmware.bin
```

## Uso

### 1. Inicialização

```
=== Inicializando Takt Time Receptor ===
Configuração carregada do LittleFS
LEDS desligado
Leds configurados no GPIO 4
Buzzer desligado
Inicializado: BUZZER
Todos os dispositivos desligados
SignalizerController inicializado
Conectando ao WiFi...
Conectando Wi-Fi...
Wi-fi conectado
Endereço IP:
192.168.80.242
Setup concluído!
```

### 2. Recebendo Comando

```
Comando de configuração recebido
Aplicando takt_count recebido: 2
LEDS ligado
Buzzer desligado
Nível aplicado: 2
==========================
```

### 3. Auto-desligamento (Nível 3)

```
LEDS ligado
Buzzer ligado
Nível aplicado: 3

(após 5 segundos)

LEDS desligado
Buzzer desligado
Todos os dispositivos desligados
Nível 3 desativado após duração do alarme.
```

### 4. Publicando Comandos (Exemplo com mosquitto_pub)

```bash
# Nível 1
mosquitto_pub -h 10.110.21.162 -t "takt/device/cost-2-2408" \
  -m '{"event":"takt_ok","message":"Normal","id":"1","timestamp":1234567890.0,"takt_count":1}'

# Nível 2
mosquitto_pub -h 10.110.21.162 -t "takt/device/cost-2-2408" \
  -m '{"event":"takt_warning","message":"Atenção","id":"2","timestamp":1234567891.0,"takt_count":2}'

# Nível 3 (crítico)
mosquitto_pub -h 10.110.21.162 -t "takt/device/cost-2-2408" \
  -m '{"event":"takt_critical","message":"Atraso crítico","id":"3","timestamp":1234567892.0,"takt_count":3}'

# Desligar tudo
mosquitto_pub -h 10.110.21.162 -t "takt/device/cost-2-2408" \
  -m '{"event":"reset","message":"Reset","id":"4","timestamp":1234567893.0,"takt_count":0}'
```

## 🔍 Troubleshooting

### WiFi não conecta

1. Verifique SSID e senha
2. Confirme se a rede é 2.4GHz (ESP32 não suporta 5GHz)
3. Aumente o timeout se necessário

### MQTT não conecta

1. Verifique IP e porta do broker
2. Teste credenciais com cliente MQTT externo
3. Confirme que o broker está acessível na rede

### LEDs não acendem

1. Verifique alimentação e GND comum da fita/anel WS2811
2. Confirme ligação do pino de dados no GPIO 4
3. Teste a fita com um exemplo simples de FastLED

### Buzzer não toca

1. Verifique se é buzzer **ativo** (não passivo)
2. Confirme alimentação (5V)
3. Teste com `tone(14, 1000)`
