# VoltWatch ⚡

> **Intelligent, Real-Time Voltage & Power Monitoring System for ESP32**

[![Platform](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/)
[![Language](https://img.shields.io/badge/Language-C%2B%2B17-orange.svg)](https://isocpp.org/)
[![Toolchain](https://img.shields.io/badge/Toolchain-arduino--cli-teal.svg)](https://arduino.github.io/arduino-cli/)
[![Tests](https://img.shields.io/badge/Tests-67%20Passing-brightgreen.svg)](firmware/test)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

---

## Overview

**VoltWatch** is a decoupled monorepo monitoring platform consisting of:
1. **Headless ESP32 Metrology Sentinel (`firmware/`)**: A high-efficiency embedded service sampling dual PZEM-004T v3.0 AC power meters, executing outage detection state machines, dispatching multi-recipient SMTPS alerts over TLS, and exposing a lightweight REST API (`/api/status`, `/api/history`) and Server-Sent Events (SSE `/api/events`) with global CORS enabled. Zero static HTML/CSS/JS is stored in or served from flash memory.
2. **Standalone Web Dashboard SPA (`web/`)**: A modern, responsive single-page application designed for deployment to GitHub Pages or local execution (`file://` / `localhost`). Features dynamic IP target resolution, settings modal with `localStorage` persistence, live animated gauges, and real-time SSE streaming.

---

## Key Features

- **⚡ Dual AC Metrology (PZEM-004T v3.0)**: Dedicated Modbus-RTU channels on ESP32 Hardware `Serial2` for both Mains Grid (`0x01`) and Inverter Output (`0x02`).
- **🛡️ Outage State Machine & Debounce**: 3-cycle glitch rejection, trip/recovery hysteresis, and cold-boot outage detection.
- **🔋 Real-Time Battery Autonomy**: Dynamic remaining runtime estimation based on live inverter watt draw and battery pack capacity.
- **📧 Direct Multi-Recipient SMTPS Alerts**: TLS/STARTTLS notifications to primary and secondary emails on trip and restoration events.
- **🌐 Headless Architecture & Global CORS**: Full CORS support (`Access-Control-Allow-Origin: *`, `OPTIONS` preflight 204) allowing external frontends to seamlessly interface with the ESP32.
- **💻 Decoupled Static Dashboard**: Modern dark-mode UI with customizable ESP32 target IP, zero hardcoded credentials, and zero ESP32 flash memory overhead.
- **🧪 100% Native Unit Testability**: 67 unit tests covering metrology, state machines, ring buffer storage, SMTP payloads, and REST APIs runnable on macOS/Linux.

---

## System Architecture

```mermaid
flowchart TD
    subgraph GridPower [Grid & Inverter AC]
        Grid[Mains Grid 230V AC] --> PZEM1[PZEM-004T 0x01]
        Inverter[Inverter Output 230V AC] --> PZEM2[PZEM-004T 0x02]
    end

    subgraph ESP32 [ESP32 Headless Telemetry Engine (firmware/)]
        PZEM1 & PZEM2 -->|Serial2 Modbus-RTU| PM[PZEMManager]
        PM -->|1Hz Polling| OSM[OutageStateMachine]
        OSM -->|Trip / Restore Events| SMTP[SMTPAlertDispatcher]
        OSM -->|Outage Records| NVS[OutageStorage Ring Buffer]
        OSM & PM -->|Telemetry| API[AsyncWebServer & SSE]
        SMTP -->|SMTPS Port 465| Mail[Gmail / Proton]
    end

    subgraph Frontend [Static Web Dashboard (web/)]
        SPA[web/index.html - GitHub Pages / Local] -->|CORS REST GET /api/status| API
        SPA -->|CORS REST GET /api/history| API
        API -->|CORS SSE Stream /api/events| SPA
    end
```

## Documentation

Comprehensive documentation is organized under the [`docs/`](docs/) directory:

- 📋 **[System Requirements & Architecture Specification](docs/requirements.md)**: Dual PZEM-004T v3.0 metrology, outage detection state machine, direct SMTP alerts, and REST API.
- 📐 **[Firmware System Architecture](docs/architecture.md)**: Software design patterns, component interactions, and execution lifecycle.
- 📖 **[Developer Reference & Context](docs/project_context.md)**: Developer handbook with quick cheat sheet, coding standards, memory rules, and test guides.
- 🔌 **[Hardware Guide](hardware/README.md)**: Resistor divider sizing, circuit schematics, protection, and BOM.

---

## Project Structure

```
VoltWatch/
├── .gitignore                    # Monorepo ignore rules (firmware/build/, secrets, etc.)
├── Makefile                      # Root forwarding Makefile (make test, build, flash)
├── README.md                     # Project documentation & overview
├── docs/                         # Technical documentation & specifications
├── hardware/                     # Schematics and electrical documentation
├── web/                          # Standalone Static Web Dashboard (GitHub Pages / Local)
│   └── index.html                # Responsive single-page application with dynamic IP target
└── firmware/                     # Embedded C++/Arduino ESP32 Telemetry Engine
    ├── Makefile                  # Firmware build & test automation
    ├── firmware.ino              # Main Arduino sketch entry point
    ├── partitions.csv            # Flash partition layout
    ├── include/                  # Global headers and configurations
    │   ├── config.h              # Metrology thresholds, battery & network configs
    │   ├── secrets.example.h     # Template for Wi-Fi and SMTP credentials
    │   ├── pins.h                # Target GPIO pin definitions (Serial2, LED, Buzzer)
    │   └── types.h               # Metrology, OutageState, OutageRecord, and status structs
    ├── src/                      # Modular C++ implementation files
    │   ├── alerts/               # SMTPS TLS alert dispatcher & alert evaluation
    │   ├── core/                 # Moving average filter & core signal processing
    │   ├── display/              # Non-blocking LED & buzzer driver
    │   ├── metrology/            # Dual PZEM-004T v3.0 Modbus RTU manager with 3-retry handling
    │   ├── network/              # Non-blocking asynchronous Wi-Fi with exponential backoff
    │   ├── sensors/              # ADC reader & resistor divider scaling (legacy/aux)
    │   ├── statemachine/         # Outage detection state machine & battery estimator
    │   ├── storage/              # Outage history NVS ring-buffer with flash wear protection
    │   ├── telemetry/            # JSON serialization and serial streaming
    │   └── web/                  # Headless ESPAsyncWebServer (CORS, REST API, SSE, mDNS)
    ├── test/                     # Native C++ unit tests (67 passing)
    │   ├── Makefile              # Native test runner Makefile
    │   ├── test_framework.h      # Lightweight test assertion macros
    │   ├── test_main.cpp         # Test runner suite entry point
    │   └── test_*.cpp            # Modular unit tests
    └── scripts/                  # Automation scripts
        ├── build.sh              # Compile firmware via arduino-cli
        ├── flash.sh              # Flash binary to ESP32
        ├── monitor.sh            # Launch serial console
        └── run_tests.sh          # Run local unit tests
```

---

## Hardware Connection & Pinout

### Recommended Voltage Divider (Default: 12V DC Rail)

```
  Monitored Rail (+) ───[ 30kΩ R1 ]───┬───[ 7.5kΩ R2 ]─── Ground (GND)
                                      │
                                      ├───[ 100nF C1 ]─── Ground (GND)
                                      │
                                      └───► ESP32 GPIO 34 (ADC1_CH6)
```

### Pin Mapping

| Subsystem / Peripheral | ESP32-WROOM | ESP32-S3 | Description |
|:---|:---|:---|:---|
| **PZEM RX (Serial2 RX2)** | **GPIO 16** | GPIO 16 | Modbus-RTU RX connected to TX of PZEM #1 & #2 |
| **PZEM TX (Serial2 TX2)** | **GPIO 17** | GPIO 17 | Modbus-RTU TX connected to RX of PZEM #1 & #2 |
| **PZEM Logic VCC** | **VIN (5V)** | 5V | Powers PZEM internal optocoupler LEDs |
| **Status LED** | **GPIO 2** | GPIO 2 | Active-high status indicator |
| **Alarm Buzzer / Relay** | **GPIO 25** | GPIO 5 | Active audio buzzer / relay trigger |
| **Auxiliary DC ADC (Optional)** | **GPIO 34** | GPIO 4 | ADC1 input (safe with WiFi active) |

> [!WARNING]
> The PZEM optocouplers require 5V VCC to function, while communication runs at 3.3V logic on GPIO 16/17. If using the optional auxiliary DC voltage divider on GPIO 34, always measure with a multimeter before connecting to ensure input **never exceeds 3.3V DC**. Read [hardware/README.md](hardware/README.md) for protection diode schematics and formulas.

---

## Getting Started

### Prerequisites

- **Host Compiler**: `clang++` or `g++` with C++17 support (for running native unit tests).
- **GNU Make**: Available on macOS and Linux by default.
- **Arduino CLI**: Install via Homebrew on macOS (`brew install arduino-cli`) or official script.
- **ESP32 Core**: Installed in Arduino CLI:
  ```bash
  arduino-cli core update-index
  arduino-cli core install esp32:esp32
  ```

---

### Build & Test Commands

The repository provides a unified [Makefile](Makefile) with standard targets:

#### 1. Run Native Unit Tests (No hardware needed)
```bash
make test
```
*Compiles with `-std=c++17` and executes all 67 tests across metrology, state machine, battery autonomy, NVS storage, SMTPS alerts, CORS/PNA headers, and web asset integrity.*

#### 2. Compile Firmware for ESP32
```bash
make build
```
*Compiles `firmware/firmware.ino` for the default `esp32:esp32:esp32` board using `arduino-cli`.*

To compile for ESP32-S3:
```bash
FQBN=esp32:esp32:esp32s3 make build
```

#### 3. Upload Firmware to ESP32
Connect your ESP32 via USB and run:
```bash
make flash
```
*Auto-detects the connected USB serial port and uploads the compiled binary (`firmware.ino.bin`) at 460800 baud.* Or specify manually:
```bash
PORT=/dev/cu.usbserial-0001 FLASH_BAUD=460800 make flash
```

#### 4. Monitor Live Serial Output
```bash
make monitor
```
*Opens interactive serial monitor at 115200 baud.*

#### 5. Launch Standalone Web Dashboard
Open `web/index.html` directly in your browser or serve it via any static web server (or GitHub Pages):
```bash
open web/index.html
```
Click the connection badge / gear icon in the header to set your ESP32's local IP (e.g. `http://192.168.1.17`). Settings are saved in browser `localStorage`.

---

## Telemetry Protocol

VoltWatch outputs newline-delimited JSON strings over Serial (115200 baud).

### Periodic Telemetry (1 Hz)
```json
{
  "type": "telemetry",
  "uptime_ms": 12000,
  "v_raw": 12.348,
  "v_avg": 12.315,
  "adc": 3054,
  "status": "NORMAL"
}
```

### Alert Transition Event (Immediate)
```json
{
  "type": "alert",
  "uptime_ms": 18450,
  "voltage": 10.420,
  "old_status": "WARNING_LOW",
  "new_status": "CRITICAL_UNDERVOLT"
}
```

---

## Configuration & Calibration

Default settings are configured in [firmware/include/config.h](firmware/include/config.h).

```cpp
// Voltage Thresholds (Volts)
constexpr VoltageThresholds DEFAULT_THRESHOLDS = {
    12.0f,  // Nominal Voltage
    10.5f,  // Critical Low (Cutoff)
    11.4f,  // Warning Low
    12.8f,  // Warning High
    14.4f,  // Critical High (Over-voltage)
    0.20f   // Hysteresis window (V)
};
```

### Secrets & Wi-Fi Configuration
1. Copy [firmware/include/secrets.example.h](firmware/include/secrets.example.h) to `firmware/include/secrets.h`:
   ```bash
   cp firmware/include/secrets.example.h firmware/include/secrets.h
   ```
2. Enter your Wi-Fi SSID, Wi-Fi password, and 16-character Google App Password.
3. `firmware/include/secrets.h` is automatically excluded by `.gitignore` to prevent credential leaks.

---

## Roadmap

- [x] Initial project layout and modular C++ architecture.
- [x] Resistor divider scaling & ADC driver with mock support.
- [x] Zero-allocation moving average digital filter.
- [x] Hysteresis and debounce alert state machine.
- [x] Structured JSON telemetry over Serial.
- [x] Complete native C++ unit test suite.
- [ ] INA219 / INA226 I2C current and power monitoring module.
- [ ] SSD1306 128x64 I2C OLED live display dashboard.
- [ ] ESP-NOW / MQTT wireless telemetry over WiFi.
- [ ] Web-based configuration portal via ESP32 softAP.

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
