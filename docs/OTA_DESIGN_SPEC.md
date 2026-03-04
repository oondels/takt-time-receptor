# OTA Design Spec

## 1. Contexto e objetivo

Este documento descreve a funcionalidade de atualizacao OTA (Over-The-Air) do projeto `takt-time-receptor` no ESP32.

Objetivos principais:

- Permitir atualizacao de firmware via HTTP sem cabo.
- Proteger a operacao com autenticacao por chave (`X-OTA-Key`).
- Registrar status da ultima tentativa de OTA no LittleFS.
- Reiniciar o dispositivo apenas em caso de atualizacao bem-sucedida.
- Manter o comportamento de Wi-Fi, MQTT e sinalizadores inalterado durante operacao normal.

## 2. Arquitetura de alto nivel

Componentes envolvidos:

- `src/main.cpp`
- `src/ota/OtaServer.h`
- `src/ota/OtaServer.cpp`
- `src/config/DeviceConfig.h`
- `src/config/DeviceConfig.cpp`
- `src/config/ConfigStorage.h`
- `src/config/ConfigStorage.cpp`

Resumo de responsabilidades:

- `main.cpp`: inicializa storage, carrega config, sobe Wi-Fi/MQTT/OTA e chama loop OTA continuamente.
- `DeviceConfig`: define o modelo de configuracao e defaults, incluindo `otaKey`.
- `ConfigStorage`: persiste config em `/config.json` e controla assinatura de firmware para reset de config em firmware nova.
- `OtaServer`: expoe endpoints HTTP de OTA, valida seguranca, grava firmware via `Update`, persiste status e controla reboot.

## 3. Contrato HTTP da OTA

### Endpoint de upload

- Metodo: `POST`
- Rota: `/ota`
- Porta: `80`
- Header obrigatorio: `X-OTA-Key: <chave>`
- Formatos de payload aceitos:
- `application/octet-stream` (RAW body)
- `multipart/form-data` (upload por formulario)

Resposta de sucesso:

```json
{"ok":true,"message":"updated","bytes_written":877733}
```

Respostas de erro:

```json
{"ok":false,"error":"<codigo>","detail":"<detalhe>"}
```

### Endpoint de status

- Metodo: `GET`
- Rota: `/ota/status`

Retornos:

- Se nao houver status salvo:

```json
{"ok":false,"message":"no status"}
```

- Se houver tentativa registrada: retorna conteudo de `/ota/last.json`.

## 4. Regras de seguranca e validacao

Regras de autenticacao:

- OTA so e aceita se `deviceConfig.otaKey` estiver configurada (nao vazia).
- Requisicao deve conter header `X-OTA-Key`.
- Valor do header deve ser igual a `deviceConfig.otaKey`.

Regras de payload:

- Se `Content-Length` existir, deve ser maior que zero.
- `Update.begin()` e chamado com `Content-Length` quando disponivel.
- Sem `Content-Length`, usa `UPDATE_SIZE_UNKNOWN`.
- Cada chunk e escrito com `Update.write()`.

Regras de falha:

- Qualquer erro de escrita/finalizacao dispara `Update.abort()`.
- Em falha, o dispositivo nao reinicia.
- Em falha, o endpoint retorna JSON com erro e detalhe.

Regra de reboot:

- Reinicia apenas apos `Update.end(true)` bem-sucedido e resposta HTTP 200 enviada.

## 5. Fluxo interno de funcionamento

Fluxo simplificado:

1. Cliente chama `POST /ota`.
2. Callback de upload detecta tipo de payload:
- Multipart: usa `otaServer.upload()`.
- RAW: usa `otaServer.raw()`.
3. No inicio da transferencia:
- Estado OTA e resetado.
- Marca `started_at_ms`.
- Captura `remote_ip`.
- Salva status `in_progress` em `/ota/last.json`.
- Valida chave OTA e prepara `Update.begin(...)`.
4. Durante transferencia:
- Cada chunk e gravado no flash via `Update.write`.
- Acumula `bytes_received`.
5. No fim:
- `Update.end(true)` valida e finaliza.
- Em sucesso: salva status `ok`, retorna 200 e reinicia.
- Em erro: salva status `fail`, retorna erro, sem reboot.

## 6. Maquina de estado OTA (logica de runtime)

Estados internos relevantes (`OtaRequestState`):

- `started`
- `uploadAccepted`
- `updateBegun`
- `updateCompleted`
- `hasError`
- `httpCode`
- `bytesWritten`
- `startedAtMs`
- `finishedAtMs`
- `error` / `detail`
- `updateErrorCode` / `updateErrorStr`
- `remoteIp`

Transicoes:

- Inicio de upload: `started=true`.
- Validacoes OK: `uploadAccepted=true`, `updateBegun=true`.
- Finalizacao OK: `updateCompleted=true`.
- Qualquer erro: `hasError=true` + metadados de erro.

## 7. Persistencia de status OTA

Arquivo de status:

- Diretorio: `/ota`
- Arquivo: `/ota/last.json`

Campos persistidos:

- `started_at_ms`
- `finished_at_ms`
- `bytes_received`
- `result` (`in_progress`, `ok`, `fail`)
- `update_error_code` (quando houver)
- `update_error_str` (quando houver)
- `remote_ip` (quando disponivel)

Momento de escrita:

- Inicio da tentativa: `result = in_progress`
- Fim com sucesso: `result = ok`
- Fim com falha: `result = fail`

## 8. Integracao com configuracao do dispositivo

Config persistida:

- Arquivo: `/config.json`
- Campo OTA: `ota_key`

Defaults:

- Definidos em `DeviceConfig.h` e aplicados por `setDefaultConfig`.

Atualizacao remota de config via MQTT:

- `applyConfigFromJson` aceita `ota_key` e salva em LittleFS.

## 9. Reset de configuracao em firmware nova

Mecanismo implementado no boot:

- Gera assinatura de firmware com `__DATE__ + __TIME__`.
- Compara com assinatura salva em `/firmware_signature.txt`.
- Se mudou, trata como firmware nova e reseta config para defaults:
- Executa `setDefaultConfig(...)`.
- Sobrescreve `/config.json`.
- Salva nova assinatura.

Resultado:

- Sempre que o binario mudar (build novo ou OTA de firmware diferente), a configuracao persistida retorna ao padrao.

## 10. Observabilidade e diagnostico

Fontes de diagnostico:

- Logs seriais (`Serial.println`) para falhas de storage e eventos de setup.
- Endpoint `GET /ota/status` para diagnostico remoto.
- Campos de erro `update_error_code` e `update_error_str`.

Erros comuns esperados:

- `unauthorized`: chave OTA ausente/incorreta.
- `ota_disabled`: `ota_key` vazia.
- `invalid_content_length`: tamanho invalido.
- `update_begin_failed`, `update_write_failed`, `update_end_failed`: falhas do processo de flash.
- `upload_aborted`: upload interrompido.

## 11. Limites e consideracoes de design

- Servidor HTTP e sincrono (`WebServer`), sem suporte multi-cliente simultaneo.
- OTA depende de conexao de rede estavel durante upload.
- Autenticacao por chave em header e simples; nao ha TLS nativo nesta implementacao.
- `Update.begin(Content-Length)` usa tamanho informado pela requisicao; com `multipart` existe overhead HTTP no tamanho total, mas o fluxo usa `Update.end(true)` para concluir com bytes efetivos escritos.

## 12. Guia operacional rapido

Build:

```bash
pio run
```

Upload OTA (RAW):

```bash
curl -X POST "http://<ip-do-esp32>/ota" \
  -H "X-OTA-Key: <key>" \
  -H "Content-Type: application/octet-stream" \
  --data-binary "@.pio/build/esp32doit-devkit-v1/firmware.bin"
```

Consultar status:

```bash
curl "http://<ip-do-esp32>/ota/status"
```

## 13. Design decisions (resumo)

- Escolha de `WebServer` por dependencia minima do core Arduino ESP32.
- Upload por streaming para evitar carregar firmware inteira em RAM.
- Persistencia de status para auditoria e troubleshooting.
- Reboot restrito ao caminho de sucesso para evitar loops de falha.
- Suporte a RAW e multipart para compatibilidade com diferentes clientes HTTP.
