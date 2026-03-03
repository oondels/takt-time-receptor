# Tutorial: Atualizacao de Firmware via OTA (HTTP)

Este guia mostra como enviar uma nova firmware para o ESP32 usando o endpoint OTA implementado no projeto.

## 1. Pre-requisitos

- Dispositivo com firmware atual conectado no Wi-Fi.
- `ota_key` configurada no `/config.json` do dispositivo.
- `curl` instalado na maquina que vai enviar a atualizacao.
- Projeto compilando via PlatformIO (`pio run`).

## 2. Como funciona no projeto

- Endpoint de upload: `POST /ota`
- Header obrigatorio: `X-OTA-Key: <sua-chave>`
- Tipo de conteudo: `application/octet-stream`
- Endpoint de status: `GET /ota/status`
- Arquivo de status salvo no ESP32: `/ota/last.json` (LittleFS)

## 3. Configurar o `ota_key` no dispositivo

No arquivo `/config.json` do ESP32, garanta que existe uma chave nao vazia:

```json
{
  "device_id": "cost-3-3608",
  "mqtt_user": "usuario",
  "mqtt_pass": "senha",
  "mqtt_server": "10.100.1.43",
  "mqtt_port": 1883,
  "takt_count": 0,
  "ota_key": "minha-chave-ota"
}
```

Se `ota_key` estiver vazia, o endpoint `/ota` retorna erro (`ota_disabled`).

## 4. Gerar o arquivo de firmware

No diretorio do projeto:

```bash
pio run
```

Arquivo gerado (padrao deste projeto):

```text
.pio/build/esp32doit-devkit-v1/firmware.bin
```

## 5. Descobrir o IP do ESP32

Use o roteador, scanner de rede, ou logs seriais:

```bash
pio device monitor -b 115200
```

Anote o IP do dispositivo, por exemplo: `192.168.1.50`.

## 6. Enviar a atualizacao OTA

Execute o comando abaixo ajustando IP, chave e caminho do binario:

```bash
curl -X POST "http://10.110.90.125/ota" \
  -H "X-OTA-Key: minha-chave-ota" \
  -H "Content-Type: application/octet-stream" \
  --data-binary "@.pio/build/esp32doit-devkit-v1/firmware.bin"
```

Resposta de sucesso esperada:

```json
{"ok":true,"message":"updated","bytes_written":877733}
```

Em sucesso, o ESP32 reinicia automaticamente.

## 7. Consultar status da ultima tentativa

```bash
curl "http://192.168.1.50/ota/status"
```

Exemplo de retorno:

```json
{
  "started_at_ms": 10482,
  "finished_at_ms": 18337,
  "bytes_received": 877733,
  "result": "ok",
  "remote_ip": "192.168.1.10"
}
```

Em falhas, podem aparecer:

- `update_error_code`
- `update_error_str`

## 8. Erros comuns e como corrigir

### 401 unauthorized

- Causa: `X-OTA-Key` ausente ou incorreta.
- Correcao: validar o header e a chave configurada em `ota_key`.

### 403 ota_disabled

- Causa: `ota_key` vazia no config.
- Correcao: definir um valor nao vazio em `/config.json`.

### 400 invalid_content_length

- Causa: upload sem tamanho valido.
- Correcao: usar `--data-binary "@arquivo.bin"` e evitar payload vazio.

### 500 update_begin_failed / update_write_failed / update_end_failed

- Causa: erro interno no processo de gravacao (imagem invalida, falta de espaco, upload interrompido).
- Correcao:
  - confirmar que o `.bin` foi gerado para o mesmo board (`esp32doit-devkit-v1`);
  - recompilar (`pio run`) e reenviar;
  - consultar `/ota/status` para detalhes de erro.

## 9. Fluxo recomendado de deploy

1. Compilar com `pio run`.
2. Confirmar conectividade com o ESP32.
3. Enviar via `curl` para `/ota`.
4. Aguardar reboot automatico.
5. Consultar `/ota/status`.
6. Validar funcionamento geral (Wi-Fi, MQTT e sinalizadores).
