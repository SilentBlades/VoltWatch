# VoltWatch - Project Context & Developer Reference

This document serves as the persistent project context, architecture manual, coding style guide, and test operational handbook for developers and AI pair programmers working on **VoltWatch**.

---

## Quick Reference Cheat Sheet

- **Project**: VoltWatch (ESP32 Headless Metrology Sentinel & Decoupled Web Dashboard)
- **Target Platform**: ESP32 (`esp32:esp32:esp32`, 115200 baud, 3.3V logic)
- **Metrology Bus**: ESP32 Hardware `Serial2` (GPIO 16 RX2, GPIO 17 TX2) for dual PZEM-004T v3.0 Modbus-RTU (`0x01` Grid, `0x02` Inverter)
- **Auxiliary Pins**: ADC1 GPIO 34 (DC analog in), GPIO 2 (Status LED), GPIO 25 (Alarm Buzzer)
- **Testing**: `make test` runs 67 native C++ unit tests locally (no hardware needed)
- **Compilation**: `make build` compiles headless firmware via `arduino-cli`
- **Flashing**: `make flash PORT=/dev/... FLASH_BAUD=460800` uploads binary to connected board
- **Serial Monitor**: `make monitor PORT=/dev/... BAUD=115200` opens serial feed at 115200 baud
- **Web SPA**: `open web/index.html` (or host via GitHub Pages with full Chrome PNA support)
- **Coding Conventions**: C++17, zero heap allocation in loop, non-blocking `millis()`, `m_` member prefix, Conventional Commits
- **Electrical Safety**: Optical isolation on PZEMs, 3.3V max on ADC pin, use ADC1 exclusively

---

## 1. Project Overview & Vision

**VoltWatch** is an embedded power metrology and outage sentinel platform structured as a decoupled monorepo:
1. **Headless ESP32 Firmware Engine (`firmware/`)**: Continuous bidirectional AC power metrology (Grid Mains input vs. Inverter output) over Modbus-RTU, instant outage trip/restoration state evaluation with 3-cycle debouncing, direct-to-inbox SMTPS TLS alerting, non-volatile outage logging with flash wear protection, and an asynchronous REST API (`/api/status`, `/api/history`) and Server-Sent Events (`/api/events`) with global CORS and Chrome Private Network Access (PNA) preflight support.
2. **Standalone Web Dashboard SPA (`web/`)**: Decoupled Single Page Application deployable to GitHub Pages or local browsers (`file://` / `localhost`). Features dynamic host resolution (`localStorage`), modal settings with quick presets, mixed-content detection banner, and inline connection diagnostics.

### Primary Use Cases
- Mains grid stability and blackout/brownout monitoring.
- Home and commercial inverter / solar battery backup runtime estimation.
- Critical infrastructure power outage alerting via direct email (no cloud broker required).
- Industrial and lab AC/DC power telemetry.

---

## 2. Core Project Goals

1. **High Reliability & Determinism**:
   - Zero dynamic heap allocation (`malloc`, `free`, `new`, `delete`, `String` reallocations) inside the main execution loop.
   - Fixed, deterministic execution latency per cycle (< 2ms total execution time per tick).
2. **Signal Integrity & Noise Rejection**:
   - 3-cycle consecutive sample debouncing (3,000 ms) prevents false alerts from transient power sags or brownouts.
   - Built-in hysteresis prevents oscillatory state chatter around decision thresholds.
   - 3-retry fault tolerance on Modbus-RTU timeouts or `NAN` readings.
3. **Cross-Platform Testability**:
   - Hardware-abstracted architecture enabling 100% of core metrology parsing, state transitions, SMTPS payloads, and JSON serialization to be compiled and unit-tested natively on macOS and Linux without hardware attached (67 passing tests).
4. **Safety & Flash Wear Protection by Design**:
   - Fail-safe state machine prioritizing over-voltage and under-voltage detection.
   - NVS writes occur strictly on state transitions (trip and restore), guaranteeing zero flash wear in the 1 Hz telemetry loop.

---

## 3. Target Environment & Hardware Specifications

- **Microcontroller**: Espressif ESP32 (ESP32-D0WDQ6, ESP32-WROOM-32, ESP32-S3)
- **Target FQBN**: `esp32:esp32:esp32` (Default)
- **Clock Frequency**: 240 MHz (Standard ESP32 dual-core)
- **Metrology Bus**: Hardware `Serial2` (GPIO 16 RX, GPIO 17 TX), 9600 baud, 8N1 Modbus-RTU
- **Logic Level**: 3.3V CMOS (PZEM optical isolators powered from 5V VIN)
- **Serial Telemetry**: 115200 baud, 8N1

---

## 4. Repository Layout

```
VoltWatch/
├── .gitignore                    # Monorepo ignore rules (firmware/build/, secrets, etc.)
├── Makefile                      # Root forwarding Makefile (make test, build, flash)
├── README.md                     # High-level presentation, hardware guide, getting started
├── docs/                         # Technical documentation & project specifications
│   ├── README.md                 # Documentation index
│   ├── architecture.md           # System dataflow, sequence diagrams, and memory model
│   ├── project_context.md        # Comprehensive project goals, architecture, conventions, tests
│   └── requirements.md           # Metrology, dual-PZEM, SMTP alert & API specs
├── hardware/                     # Hardware documentation and schematics
│   └── README.md                 # Circuit diagrams, divider formulas, PZEM wiring, BOM
├── web/                          # Standalone Static Web Dashboard (GitHub Pages / Local)
│   └── index.html                # Responsive single-page application with dynamic IP target
└── firmware/                     # Embedded C++/Arduino ESP32 Telemetry Engine
    ├── Makefile                  # Firmware build & test automation
    ├── firmware.ino              # Main Arduino sketch entry point
    ├── partitions.csv            # Flash partition layout
    ├── include/                  # Headers, configuration, and types
    │   ├── config.h              # Base system configuration and thresholds
    │   ├── secrets.example.h     # Template for credentials and Wi-Fi/SMTP settings
    │   ├── pins.h                # Board-specific GPIO pin assignments (Serial2, LED, Buzzer)
    │   └── types.h               # Metrology, OutageState, OutageRecord structs
    ├── src/                      # Modular C++ implementation files
    │   ├── alerts/               # SMTPS TLS alert dispatcher & alert evaluation
    │   ├── core/                 # Digital filtering and core algorithms
    │   ├── display/              # LED indicator and audible alarm patterns
    │   ├── metrology/            # Dual PZEM-004T v3.0 Modbus RTU manager with 3-retry handling
    │   ├── network/              # Non-blocking asynchronous Wi-Fi with exponential backoff
    │   ├── sensors/              # ADC abstraction and voltage divider math
    │   ├── statemachine/         # Outage detection state machine & battery estimator
    │   ├── storage/              # Outage history NVS ring-buffer with flash wear protection
    │   ├── telemetry/            # JSON serialization and serial dispatching
    │   └── web/                  # Headless ESPAsyncWebServer (CORS, PNA, REST API, SSE, mDNS)
    ├── test/                     # Native C++ unit tests (67 passing)
    │   ├── Makefile              # Test runner Makefile
    │   ├── test_framework.h      # Zero-dependency micro test runner
    │   ├── test_main.cpp         # Unit test entry point
    │   └── test_*.cpp            # Modular unit tests
    └── scripts/                  # Developer automation scripts
        ├── build.sh              # Compile firmware via arduino-cli
        ├── flash.sh              # Upload firmware to board
        ├── monitor.sh            # Launch serial monitor
        └── run_tests.sh          # Compile and execute native unit test suite
```

---

## 5. Coding Standards & Conventions

### 5.1 Language Standard & Compiler Dialect
- C++17 (supported by modern ESP32 Arduino Core 2.x/3.x and native Apple Clang / GCC).
- Code must compile with `-Wall -Wextra -Werror` clean without warnings.

### 5.2 Naming Conventions
- **Types / Classes / Structs / Enums**: `PascalCase` (e.g., `MovingAverageFilter`, `VoltageReading`, `AlertLevel`).
- **Namespaces**: `PascalCase` (e.g., `VoltWatch::Config`).
- **Methods & Functions**: `camelCase` (e.g., `calculateVoltage()`, `addSample()`).
- **Constants & Enums Values**: `ALL_CAPS_SNAKE` (e.g., `CRITICAL_OVERVOLT`, `SERIAL_BAUD_RATE`).
- **Member Variables**: Prefix with `m_` in `camelCase` (e.g., `m_pin`, `m_currentLevel`).
- **Global / Static Variables**: Prefix with `g_` in `camelCase` (e.g., `g_sensor`, `g_filter`).

### 5.3 Memory & Embedded Best Practices
- **No Heap Reallocation**: Never instantiate `String` dynamically inside `loop()`. Use fixed-size character arrays (`char buffer[N]`) and `snprintf`.
- **Pass by Const Reference**: Pass structs and configuration objects by `const Type&` to avoid stack copying overhead.
- **Const Correctness**: Mark all non-mutating member functions `const`. Mark immutable configuration variables `constexpr` or `const`.
- **Zero Blocking**: Never call `delay()` inside `loop()`. Use non-blocking elapsed time checks:
  ```cpp
  if (currentMillis - lastTime >= INTERVAL_MS) {
      lastTime = currentMillis;
      // Do task
  }
  ```

---

## 6. Git & Version Control Conventions

### 6.1 Branching Strategy
- `main`: Production-ready, fully tested firmware releases.
- `feat/<feature-name>`: New feature implementations.
- `fix/<bug-name>`: Bug fixes and issue patches.
- `docs/<topic>`: Documentation, guides, or schematic updates.

### 6.2 Conventional Commits
All commits should follow the Conventional Commits specification:
- `feat(core)`: Add new functionality.
- `fix(alerts)`: Fix threshold or debouncing bug.
- `test(sensor)`: Add or modify unit tests.
- `docs(hardware)`: Update wiring schematics or README.
- `refactor(telemetry)`: Code cleanup without behavior change.
- `chore(build)`: Toolchain, Makefile, or CI scripts update.

### 6.3 Sensitive Files & Secrets
- Never commit `secrets.h`, `config.local.h`, `.env`, or real WiFi/SMTP passwords.
- Always modify `firmware/include/secrets.example.h` when adding new configurable secret parameters.
- `firmware/include/secrets.h` and `firmware/**/secrets.h` are strictly ignored by `.gitignore`.

---

## 7. Build, Test, and Flashing Commands

### 7.1 Native Unit Tests (Local Machine - No Hardware Needed)
Execute test suite locally using standard C++17 compiler (67 tests):
```bash
# Using root Makefile
make test

# Or directly inside firmware directory
cd firmware && make test
```

### 7.2 Firmware Compilation (Arduino CLI)
Compile the headless firmware for the ESP32 target:
```bash
# Default ESP32 board
make build

# Or specify a custom FQBN (e.g., ESP32-S3)
FQBN=esp32:esp32:esp32s3 make build

# Or directly inside firmware directory
cd firmware && make build
```

### 7.3 Flashing to Hardware
Upload binary (`firmware.ino.bin`) to connected ESP32 board:
```bash
# Auto-detects USB serial port with default reliable 460800 baud
make flash

# Or specify explicit port and custom baud
make flash PORT=/dev/cu.usbserial-0001 FLASH_BAUD=460800
```

### 7.4 Serial Monitor
Open interactive serial console:
```bash
# Auto-detects port at 115200 baud
make monitor

# Or specify port
make monitor PORT=/dev/cu.usbserial-0001 BAUD=115200
```

### 7.5 Launching Standalone Web Dashboard
Open the decoupled Single Page Application in your browser or host it via GitHub Pages:
```bash
open web/index.html
```
Click the connection badge / gear icon in the top header to configure the target ESP32 IP address (e.g. `http://192.168.1.17`). Settings are saved in browser `localStorage`. Includes built-in Chrome Private Network Access (PNA) and mixed-content detection.

---

## 8. Electrical & Safety Guidelines

1. **Strict 3.3V Input Bound**: Never expose ESP32 GPIO to voltages above 3.3V. Always calculate divider outputs to include at least a 20% margin above maximum expected voltage.
2. **Use ADC1 Exclusively**: ESP32 ADC2 channels are multiplexed with the WiFi subsystem. Always assign sensor inputs to ADC1 channels (GPIO 32, 33, 34, 35, 36, 39).
3. **Transient Suppression**: Always equip voltage divider inputs with a low-pass ceramic capacitor (100nF) and a clamping Schottky diode (BAT54S) to absorb electrical transients.
