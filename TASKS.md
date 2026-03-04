# TASKS.md

## Projeto: OTA Trigger via MQTT + Servidor Firmware

---

## TASK 1
name: Detectar mensagem update_takt_time via MQTT
conclude: true

### Objetivo
Adicionar detecção da mensagem MQTT com título "update_takt_time" no fluxo atual de recebimento.

### Requisitos
- Identificar no código atual onde mensagens MQTT são tratadas.
- Detectar mensagem quando:
  - event == "update_takt_time"
  OU
  - message == "update_takt_time"
- Extrair campo obrigatório:
  - update_url (string)
- Validar:
  - Não vazio
  - Começa com "http://" ou "https://"

### Regras
- NÃO executar OTA dentro do callback MQTT.
- NÃO bloquear loop principal.
- NÃO modificar lógica existente de outros comandos.

### Ação Esperada
- Criar flag global ou estrutura para armazenar:
  - pendingUpdateUrl
  - pendingUpdateTimestamp
  - otaPending = true
- Apenas sinalizar a atualização.

### Critério de Aceite
- Projeto compila com `pio run`
- Receber update_takt_time não quebra outros comandos
- Caso update_url esteja ausente, logar erro e NÃO travar

---

## TASK 2
name: Implementar módulo OTA via HTTP (cliente)
conclude: true

### Objetivo
Criar módulo responsável por baixar firmware via HTTP e aplicar Update.

### Arquitetura
Criar:
- src/ota/OtaMqttTrigger.h
- src/ota/OtaMqttTrigger.cpp

### Implementar função:
bool triggerOtaFromUrl(DeviceConfig& cfg, const String& updateUrl);

### Requisitos Técnicos
- Usar HTTPClient
- Usar WiFiClient
- Stream de download (buffer 1KB–4KB)
- Update.begin()
- Update.write()
- Update.end(true)

### Tratamento de Erros
- Se HTTP != 200 → abortar
- Se Update falhar → Update.abort()
- Nunca rebootar em caso de falha

### Persistência
Atualizar /ota/last.json contendo:
- started_at_ms
- finished_at_ms
- bytes_received
- result ("ok" ou "fail")
- error
- update_url

### Critério de Aceite
- Compila
- Falha de download gera status fail
- Sucesso chama Update.end(true)

---

## TASK 3
name: Agendar execução OTA fora do callback MQTT
conclude: true

### Objetivo
Executar OTA no loop principal, não no callback MQTT.

### Requisitos
- Se otaPending == true:
  - Verificar WiFi conectado
  - Executar triggerOtaFromUrl()
  - Resetar flag
- Garantir execução única (single-flight)
- Proteger contra múltiplos disparos simultâneos

### Regras
- NÃO bloquear loop indefinidamente
- NÃO quebrar reconexão MQTT
- NÃO iniciar OTA se já estiver em andamento

### Critério de Aceite
- Compila
- OTA executa apenas no loop()
- Não executa múltiplas vezes para mesma mensagem

---

## TASK 4
name: Criar servidor Node.js minimalista para firmware
conclude: true

### Objetivo
Criar servidor simples que entregue firmware.bin.

### Estrutura
Criar pasta:
tools/ota-server/

Arquivos:
- server.js
- package.json

### Comportamento
- GET /update-takttime
- Retorna firmware.bin
- Content-Type: application/octet-stream
- Content-Length correto
- Porta: process.env.PORT || 2399
- Firmware path: process.env.FIRMWARE_PATH || "./firmware.bin"

### Regras
- NÃO usar banco de dados
- NÃO usar docker
- NÃO adicionar autenticação
- Código deve ser simples e direto

### Critério de Aceite
- node server.js inicia servidor
- curl http://localhost:2399/update-takttime baixa firmware.bin

---

## TASK 5
name: Validar fluxo completo OTA via MQTT
conclude: true

### Objetivo
Garantir funcionamento integrado.

### Cenário de Teste
1. Rodar servidor Node
2. Publicar mensagem MQTT:
   {
     "event": "update_takt_time",
     "update_url": "http://<ip>:2399/update-takttime",
     "timestamp": <timestamp>
   }

3. Dispositivo deve:
   - Detectar mensagem
   - Agendar OTA
   - Baixar firmware
   - Atualizar /ota/last.json
   - Rebootar

### Regras
- NÃO modificar arquitetura
- NÃO alterar OTA HTTP existente
- NÃO adicionar logs excessivos

### Critério de Aceite
- Compila
- Fluxo executa sem travar MQTT
- last.json reflete resultado
- Reboot ocorre apenas em sucesso

---

# FIM DAS TASKS
