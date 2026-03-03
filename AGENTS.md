# Repository Guidelines

## Project Structure & Module Organization
This repository contains ESP32 firmware built with PlatformIO.

- `src/main.cpp`: application entry point and runtime orchestration.
- `src/wifi/`: Wi-Fi connection and reconnection logic.
- `src/mqtt/`: MQTT connection, subscriptions, and message parsing.
- `src/config/`: device configuration models and LittleFS persistence.
- `src/sinalizer/`: LED/buzzer device control and level controller.
- `include/`: shared headers.
- `lib/`: local libraries (if needed).
- `test/`: PlatformIO unit tests.

Keep new features modular: add a dedicated folder under `src/` with matching `.h` and `.cpp` files.

## Build, Test, and Development Commands
- `pio run` — compile firmware for the configured board (`esp32doit-devkit-v1`).
- `pio run -t upload` — flash firmware to the connected ESP32.
- `pio device monitor -b 115200` — open serial monitor.
- `pio run -t upload && pio device monitor -b 115200` — flash and monitor in one workflow.
- `pio test` — run tests from the `test/` directory.

## Coding Style & Naming Conventions
- Use C++ (Arduino framework) with 2-space indentation.
- Place opening braces on a new line, following existing files.
- Use PascalCase for classes (`WifiClient`, `SignalizerController`).
- Use camelCase for functions/variables (`processarComando`, `mqttClient`).
- Prefer `constexpr`/`const` for constants and explicit types for embedded reliability.

No formatter or linter is currently configured; keep changes consistent with nearby code.

## Testing Guidelines
Use PlatformIO Unit Testing (`pio test`) for logic that can run without hardware (config parsing, command validation, state transitions). Name test files `test_<module>.cpp`.

There is no strict coverage threshold yet; aim to cover new logic and regressions introduced by your change.

For hardware-dependent behavior (Wi-Fi, MQTT broker, LEDs, buzzer), include manual verification steps and expected serial output in the PR.

## Commit & Pull Request Guidelines
Follow Conventional Commit style observed in history:
- `feat: ...`
- `fix: ...`
- `docs: ...`
- Optional scope, e.g. `fix(sinalizer): ...`

PRs should include: summary, motivation, test evidence (`pio run`, `pio test`, and/or hardware checks), linked issue/task, and logs/screenshots when behavior changes are user-visible.

## Security & Configuration Tips
Never commit real Wi-Fi or MQTT credentials. Use placeholders in examples and sanitize logs before sharing.
