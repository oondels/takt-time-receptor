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
- [Uso](#uso)

## Visão Geral

O **Takt Time Receptor** é um dispositivo IoT baseado em ESP32 que recebe comandos via MQTT para controlar um sistema de sinalização progressiva composto por 3 LEDs e 1 buzzer. O sistema foi projetado para alertar operadores sobre o status do takt time em processos produtivos.

### Funcionamento

1. Conecta-se à rede WiFi configurada
2. Estabelece conexão com broker MQTT
3. Escuta comandos no tópico `takt/device/{DEVICE_ID}`
4. Processa comandos JSON e ativa níveis de sinalização
5. Desativa automaticamente após tempo configurado no nível máximo

## Características

- **Conexão WiFi**: Auto-reconexão automática em caso de perda de sinal
- **Comunicação MQTT**: Processamento automático de mensagens JSON
- **Sinalização Progressiva**: 4 níveis de alerta (0-3)
- **Validação de Dependências**: Garante ativação sequencial dos LEDs
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
│  │   ESP32      │  │   Broker     │  │  LED1-3 +    │  │
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
- Suporta lógica invertida para LEDs (LOW=ligado)
- Timer interno para controle de duração
- Métodos: `activate()`, `deactivate()`, `isActive()`

#### 4. **SignalizerController**
- Orquestra múltiplos Sinalizers
- Implementa lógica de níveis progressivos
- Valida dependências entre níveis
- Auto-desligamento após duração configurável

## 🔧 Hardware

### Componentes Necessários

| Componente | Quantidade | Especificação |
|------------|------------|---------------|
| ESP32 DevKit v1 | 1 | Qualquer variante |
| LEDs | 3 | Cor: Verde, Amarelo, Vermelho (sugestão) |
| Buzzer Ativo | 1 | 5V, 1000Hz |
| Resistores | 3 | 220Ω para LEDs |
| Protoboard | 1 | Opcional |
| Jumpers | - | Conforme necessário |

### Pinagem

```cpp
GPIO 25 → LED1 (Verde - Nível 1)
GPIO 26 → LED2 (Amarelo - Nível 2)
GPIO 27 → LED3 (Vermelho - Nível 3)
GPIO 14 → Buzzer (Alarme)
GND     → Comum
```

### Diagrama de Conexão

```
ESP32                    Componentes
─────                    ───────────
GPIO 25 ──[220Ω]──┬──── LED1 (+)
                  └──── LED1 (-) → GND

GPIO 26 ──[220Ω]──┬──── LED2 (+)
                  └──── LED2 (-) → GND

GPIO 27 ──[220Ω]──┬──── LED3 (+)
                  └──── LED3 (-) → GND

GPIO 14 ──────────┬──── Buzzer (+)
                  └──── Buzzer (-) → GND
```

> **Nota**: Os LEDs usam lógica invertida (LOW = ligado, HIGH = desligado)

## Configuração

### 1. Credenciais WiFi

```cpp
const char *SSID = "SUA_REDE_WIFI";
const char *PASSWORD = "SUA_SENHA";
```

### 2. Configuração MQTT

```cpp
const char *MQTT_SERVER = "IP_DO_BROKER";
const int MQTT_PORT = 1883;
const char *MQTT_USER = "usuario";
const char *MQTT_PASS = "senha";
const char *DEVICE_ID = "seu-device-id";
```

### 3. Persistência e Config Remota

- As configurações de dispositivo são salvas em **LittleFS** (arquivo `/config.json`).
- No boot, o dispositivo carrega o arquivo e aplica as configurações.
- Atualizações podem ser feitas via MQTT, salvando automaticamente no LittleFS.
- O `DEVICE_ID` padrão para configuração remota é **cost-fab-cel**.

### 3. Duração do Alarme

```cpp
// No construtor do buzzer (em milissegundos)
Sinalizer buzzer("Buzzer", BUZZER, TipoSinalizador::BUZZER, 1000, 5000);
//                                                           freq  duração
```

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

Exemplo Teste de funcionalidade
```json
{
  "event": "takt_alert",
  "message": "test_takt_system",
  "id": "3-3508",
  "timestamp": 1698412800.5,
  "takt_count": 1
}
```

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
  "mqtt_server": "10.100.1.43",
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

## Níveis de Sinalização

### Tabela de Comandos

| Comando | Nível | LED1 | LED2 | LED3 | Buzzer | Descrição |
|---------|-------|------|------|------|--------|-----------|
| `0` | DESLIGADO | ❌ | ❌ | ❌ | ❌ | Tudo desligado |
| `1` | NÍVEL_1 | ✅ | ❌ | ❌ | ❌ | Alerta inicial |
| `2` | NÍVEL_2 | ✅ | ✅ | ❌ | ❌ | Atenção aumentada |
| `3` | NÍVEL_3 | ✅ | ✅ | ✅ | ✅ | **Crítico + Alarme** |
| `99` | SEQUÊNCIA | 🔄 | 🔄 | 🔄 | 🔄 | Sequência completa (demo) |

### Regras de Validação

#### Dependências entre Níveis

1. **LED2** só pode ser ativado se **LED1** estiver ligado
2. **LED3** só pode ser ativado se **LED1 E LED2** estiverem ligados
3. **Buzzer** só ativa no **Nível 3** (todos LEDs acesos)

#### Auto-desligamento

- No **Nível 3**, após `activationDuration` (padrão: 5000ms), o sistema **desliga automaticamente** todos os dispositivos
- O timer é resetado a cada nova ativação do Nível 3

### Fluxo de Ativação

```
Comando 1 → LED1 ON
            ↓
Comando 2 → LED1 ON + LED2 ON
            ↓
Comando 3 → LED1 ON + LED2 ON + LED3 ON + BUZZER ON
            ↓ (após 5 segundos)
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
│   └── sinalizer/
│       ├── Signalizer.h            # Header dispositivo individual
│       ├── Signalizer.cpp          # Implementação dispositivo
│       ├── SignalizerController.h  # Header controlador
│       └── SignalizerController.cpp # Implementação controlador
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
lib_deps = 
    knolleary/PubSubClient@^2.8
    bblanchon/ArduinoJson@^6.21.3
monitor_speed = 115200
```

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

## Uso

### 1. Inicialização

```
=== Inicializando Takt Time Receptor ===
Inicializado: LED LED1 no pino 25
Inicializado: LED LED2 no pino 26
Inicializado: LED LED3 no pino 27
Inicializado: BUZZER Buzzer no pino 14
SignalizerController inicializado
Conectando ao WiFi...
WiFi conectado! IP: 192.168.1.100
Iniciando cliente MQTT...
Conectado ao broker MQTT
Setup concluído!
```

### 2. Recebendo Comando

```
=== Nova mensagem MQTT ===
Event: takt_warning
Message: Produção atrasada
ID: msg-001
Command: 2
==========================

LED1 ligado
LED2 ligado
Nível aplicado: 2
```

### 3. Auto-desligamento (Nível 3)

```
LED1 ligado
LED2 ligado
LED3 ligado
Buzzer ligado
Nível aplicado: 3
Nível 3 ativado. Aguardando duração do alarme...

(após 5 segundos)

LED1 desligado
LED2 desligado
LED3 desligado
Buzzer desligado
Todos os dispositivos desligados
Nível 3 desativado automaticamente após duração do alarme.
```

### 4. Publicando Comandos (Exemplo com mosquitto_pub)

```bash
# Nível 1
mosquitto_pub -h 10.110.21.162 -t "takt/device/cost-2-2408" \
  -m '{"event":"takt_ok","message":"Normal","id":"1","timestamp":1234567890.0,"command":1}'

# Nível 2
mosquitto_pub -h 10.110.21.162 -t "takt/device/cost-2-2408" \
  -m '{"event":"takt_warning","message":"Atenção","id":"2","timestamp":1234567891.0,"command":2}'

# Nível 3 (crítico)
mosquitto_pub -h 10.110.21.162 -t "takt/device/cost-2-2408" \
  -m '{"event":"takt_critical","message":"Atraso crítico","id":"3","timestamp":1234567892.0,"command":3}'

# Desligar tudo
mosquitto_pub -h 10.110.21.162 -t "takt/device/cost-2-2408" \
  -m '{"event":"reset","message":"Reset","id":"4","timestamp":1234567893.0,"command":0}'
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

1. Verifique polaridade (lógica invertida: LOW=ligado)
2. Confirme resistores de 220Ω
3. Teste pinos individualmente com código simples

### Buzzer não toca

1. Verifique se é buzzer **ativo** (não passivo)
2. Confirme alimentação (5V)
3. Teste com `tone(14, 1000)`