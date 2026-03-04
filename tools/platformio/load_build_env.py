import os
from pathlib import Path

from SCons.Script import Import

Import("env")


def parse_dotenv(dotenv_path: Path):
  values = {}
  if not dotenv_path.is_file():
    return values

  for raw_line in dotenv_path.read_text(encoding="utf-8").splitlines():
    line = raw_line.strip()
    if not line or line.startswith("#"):
      continue

    if line.startswith("export "):
      line = line[7:].strip()

    if "=" not in line:
      continue

    key, value = line.split("=", 1)
    key = key.strip()
    value = value.strip()

    if not key:
      continue

    if value and value[0] in ("'", '"') and value[-1] == value[0]:
      value = value[1:-1]
    elif " #" in value:
      value = value.split(" #", 1)[0].rstrip()

    values[key] = value

  return values


def as_cpp_string(value: str):
  escaped = value.replace("\\", "\\\\").replace('"', '\\"')
  return f'\\"{escaped}\\"'


project_dir = Path(env["PROJECT_DIR"])
dotenv_path = project_dir / ".env"
dotenv_values = parse_dotenv(dotenv_path)


def get_setting_with_source(key: str, fallback: str):
  if key in os.environ:
    return os.environ[key], "env"
  if key in dotenv_values:
    return dotenv_values[key], ".env"
  return fallback, "fallback"


def mask_secret(value: str):
  if not value:
    return "<vazio>"
  if len(value) <= 4:
    return "*" * len(value)
  return f"{value[:2]}***{value[-2:]}"


definitions = [
  ("WIFI_SSID", "BUILD_WIFI_SSID", "wifi-ssid-not-set"),
  ("WIFI_PASSWORD", "BUILD_WIFI_PASSWORD", "wifi-password-not-set"),
  ("DEFAULT_MQTT_USER", "BUILD_DEFAULT_MQTT_USER", "mqtt-user-not-set"),
  ("DEFAULT_MQTT_PASS", "BUILD_DEFAULT_MQTT_PASS", "mqtt-pass-not-set"),
  ("DEFAULT_MQTT_SERVER", "BUILD_DEFAULT_MQTT_SERVER", "mqtt-server-not-set"),
  ("DEFAULT_OTA_KEY", "BUILD_DEFAULT_OTA_KEY", "default-ota-key"),
]

cpp_defines = []
missing_keys = []
loaded_values = {}
sensitive_keys = {"WIFI_PASSWORD", "DEFAULT_MQTT_PASS", "DEFAULT_OTA_KEY"}

for env_key, define_key, fallback in definitions:
  value, source = get_setting_with_source(env_key, fallback)
  if value == fallback:
    missing_keys.append(env_key)
  cpp_defines.append((define_key, as_cpp_string(value)))
  loaded_values[env_key] = (value, source)

mqtt_port_raw, mqtt_port_source = get_setting_with_source("DEFAULT_MQTT_PORT", "1883")
try:
  mqtt_port = int(mqtt_port_raw)
except ValueError:
  print(f"[build_env] DEFAULT_MQTT_PORT inválida ({mqtt_port_raw}). Usando 1883.")
  mqtt_port = 1883

loaded_values["DEFAULT_MQTT_PORT"] = (str(mqtt_port), mqtt_port_source)
cpp_defines.append(("BUILD_DEFAULT_MQTT_PORT", mqtt_port))
env.Append(CPPDEFINES=cpp_defines)

if dotenv_path.is_file():
  print("[build_env] .env carregado para defines de compilação.")
else:
  print("[build_env] .env não encontrado. Usando variáveis do ambiente e fallbacks.")

if missing_keys:
  joined = ", ".join(missing_keys)
  print(f"[build_env] Atenção: usando fallback para: {joined}")

print("[build_env] Variáveis carregadas:")
for key in (
    "WIFI_SSID",
    "WIFI_PASSWORD",
    "DEFAULT_MQTT_USER",
    "DEFAULT_MQTT_PASS",
    "DEFAULT_MQTT_SERVER",
    "DEFAULT_MQTT_PORT",
    "DEFAULT_OTA_KEY",
):
  value, source = loaded_values[key]
  display = mask_secret(value) if key in sensitive_keys else value
  print(f"[build_env]  - {key} ({source}): {display}")
