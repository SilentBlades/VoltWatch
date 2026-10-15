# VoltWatch: System Requirements & Architecture Specification

## 1. Project Overview & Philosophy
* **Project Name:** VoltWatch
* **Target Hardware:** Espressif ESP32 Dev Module (ESP-WROOM-32, dual-core Xtensa LX6)
* **Core Philosophy:** **Zero Server Dependency.** The system is entirely self-hosted, edge-computed, and maintenance-free. It does not depend on local home servers (Mac mini, Raspberry Pi), Docker containers, MQTT brokers, or paid third-party cloud IoT platforms (Blynk, Adafruit IO, AWS IoT).
* **Primary Function:** Continuous bidirectional AC power metrology (Grid Mains input vs. Inverter output), instant outage detection, direct-to-inbox multi-recipient SMTP dispatch, and embedded local telemetry delivery.

---

## 2. Hardware Architecture & Pinout

### 2.1 Hardware Components
1. **Microcontroller:** ESP32 NodeMCU / DevKit V1 (30-pin or 38-pin, 3.3V logic).
2. **Metrology Module 1 (Grid Mains):** Peacefair PZEM-004T v3.0 (AC 80–260V, 0–100A CT clamp).
   * Modbus RTU Address: `0x01`
3. **Metrology Module 2 (Inverter Output):** Peacefair PZEM-004T v3.0 (AC 80–260V, 0–100A CT clamp).
   * Modbus RTU Address: `0x02`
4. **Power Supply:** Isolated 5V DC step-down power supply (or AC-to-DC 5V 2A micro-USB/Type-C adapter) backed by the inverter output to guarantee continuous ESP32 operation during grid failure.

### 2.2 UART Bus Topology
Both PZEM-004T v3.0 modules communicate over TTL UART using Modbus-RTU over a shared bus or dedicated hardware serial lines.

* **Bus Interface:** ESP32 Hardware `Serial2`
  * **ESP32 GPIO 16 (RX2):** Connected to TX of PZEM #1 and PZEM #2
  * **ESP32 GPIO 17 (TX2):** Connected to RX of PZEM #1 and PZEM #2
* **PZEM Power Connections:**
  * **Logic VCC (5V):** Connected to ESP32 `VIN` / 5V rail.
  * **GND:** Common system ground.
  * *Note on PZEM 5V Logic:* The optical isolators on the PZEM-004T v3.0 board function reliably with 3.3V GPIO levels on ESP32, but VCC requires 5V to drive the optocoupler LEDs.

```
   +-------------------------------------------------------+
   |                   ESP32 Dev Module                    |
   |                                                       |
   |   GPIO 16 (RX2) <---+------------------+              |
   |   GPIO 17 (TX2) ----+--------+         |              |
   |   5V (VIN) ---------+---+    |         |              |
   |   GND --------------+---|----+----+    |              |
   +---------------------|---|----|----|----+--------------+
                         |   |    |    |    |
               +---------+   |    |    +----+---------+
               |             |    |                   |
        +------v------+      |    |            +------v------+
        | PZEM #1     |      |    +----------> | PZEM #2     |
        | (Grid)      |      |                 | (Inverter)  |
        | Addr: 0x01  | <----+                 | Addr: 0x02  |
        +-------------+                        +-------------+
        | AC In: Grid |                        | AC In: Inv  |
        +-------------+                        +-------------+
```

---

## 3. Metrology & State Machine

### 3.1 Polling & Metrics Acquisition
Every **1000 ms**, the ESP32 polls both PZEM modules sequentially:
* **Grid Module (`0x01`):**
  * Voltage ($V_{\text{grid}}$) [V]
  * Frequency ($f_{\text{grid}}$) [Hz]
* **Inverter Output Module (`0x02`):**
  * Active Power ($P_{\text{load}}$) [W]
  * Current ($I_{\text{load}}$) [A]
  * Power Factor ($PF_{\text{load}}$)
  * Cumulative Energy ($E_{\text{load}}$) [kWh]
  * Output Voltage ($V_{\text{inv}}$) [V]

### 3.2 State Detection & Debouncing
To prevent false-positive alerts caused by grid brownouts, voltage sags, or transient noise:
* **Thresholds:**
  * Grid Present: $V_{\text{grid}} \ge 170.0\text{ V}$
  * Grid Outage: $V_{\text{grid}} < 80.0\text{ V}$ (or Modbus read timeout/NaN)
* **Debounce Window:** State change must persist for **3 consecutive polling cycles (3,000 ms)** before a transition is committed.
* **States:**
  * `STATE_NORMAL`: Grid is healthy and supplying loads.
  * `STATE_OUTAGE`: Grid dropped below threshold; running on battery/inverter.
  * `STATE_RECOVERED`: Grid restored above operational threshold after an outage.

---

## 4. Alerting Engine (Direct SMTP)

### 4.1 Transport & Security
* **Protocol:** SMTPS over TLS (Port 465) or STARTTLS (Port 587) directly from the ESP32 network stack.
* **Server:** `smtp.gmail.com`
* **Authentication:** Google Account + 16-character Google App Password (bypasses interactive OAuth/2FA).
* **NTP Synchronization:** ESP32 syncs local time via NTP (`pool.ntp.org`, timezone `IST / Asia/Kolkata` / UTC+5:30) on boot to timestamp alert events accurately.

### 4.2 Recipient Routing
Alerts are sent simultaneously in a single SMTP session to:
1. Primary Phone (Proton Mail)
2. Primary Phone (Gmail)
3. Secondary Phone / Spouse (Gmail)

### 4.3 Alert Payloads & Formats
* **Outage Trigger (`STATE_NORMAL` $\to$ `STATE_OUTAGE`):**
  * **Subject:** `🚨 [ALERT] Power Outage Detected - Grid Down`
  * **Body:** Plain text + HTML snapshot including:
    * Exact timestamp (IST)
    * Grid voltage prior to trip
    * Live inverter active load ($W$), current ($A$), and power factor
    * Estimated remaining battery runtime at instantaneous load
* **Restoration Trigger (`STATE_OUTAGE` $\to$ `STATE_RECOVERED`):**
  * **Subject:** `✅ [RESTORED] Grid Power Resumed`
  * **Body:**
    * Restoration timestamp
    * Total outage duration ($H\text{h } M\text{m } S\text{s}$)
    * Total battery energy consumed ($kWh$) during the outage
    * Current grid line voltage and frequency

---

## 5. Local Telemetry & Interface Requirements

### 5.1 Headless Asynchronous Web Server & API Pipeline
* **Architecture:** **Headless Metrology API.** The ESP32 acts strictly as an edge sensor and JSON API/SSE server; it serves zero static HTML/CSS/JS assets from flash memory.
* **Web Server:** `ESPAsyncWebServer` running on port 80.
* **Global CORS & Chrome Private Network Access (PNA):**
  * Allows public web origins (such as GitHub Pages) and local files (`file://`, `localhost`) to query the private IP (e.g. `192.168.1.17`).
  * Injected Global Headers:
    ```
    Access-Control-Allow-Origin: *
    Access-Control-Allow-Methods: GET, POST, OPTIONS
    Access-Control-Allow-Headers: Content-Type, Access-Control-Request-Private-Network
    Access-Control-Allow-Private-Network: true
    ```
  * **HTTP OPTIONS Preflight:** Responds with HTTP `204` (No Content) and all required CORS/PNA preflight response headers.
* **Lightweight Diagnostic Payload (`/` and `/index.html`):**
  * Responds with concise JSON service health metadata:
    ```json
    {"service":"VoltWatch","status":"online","api":["/api/status","/api/history","/api/events"]}
    ```
* **Real-Time Data Pipeline:** Server-Sent Events (SSE) `/api/events` pushing status JSON every 1,000 ms to connected browser clients.
* **mDNS Resolution:** Advertised locally as `http://voltwatch.local` (port 80).

### 5.2 Standalone Web Dashboard SPA (`web/index.html`)
* **Deployment Model:** Fully decoupled Single Page Application (SPA) deployable to **GitHub Pages**, any static web host, or executed locally via `file://` or `localhost`.
* **Zero Flash Overhead:** Zero static assets stored in or served from ESP32 flash memory.
* **Dynamic Target Resolution:**
  * Checks browser `localStorage` key `'voltwatch_target_ip'`.
  * If served from the board itself, uses `window.location.origin`.
  * Defaults to `http://192.168.1.17` when running on an external host or local machine.
  * All REST queries and SSE streams dynamically prefix endpoints with this resolved `API_BASE`.
* **Interactive Settings Modal:**
  * Opened via the settings gear button embedded in the header connection badge.
  * Allows on-the-fly updates to target IP/hostname with preset buttons (`192.168.1.17`, `voltwatch.local`, `Auto / Reset`) and `localStorage` persistence.
* **Mixed-Content Detection & Guidance:**
  * Automatically detects when the dashboard is loaded over HTTPS (e.g. GitHub Pages) attempting to query an HTTP private IP.
  * Displays an inline warning banner explaining browser security policies and step-by-step instructions (Site Settings -> Insecure content -> Allow, or local execution via `file://`).
* **Inline Connection Diagnostics:**
  * Displays active polling URL (`http://<ip>/api/status`), active channel (`SSE Stream`, `HTTP Polling`), and real-time HTTP status codes or network error messages.
* **UI Features:**
  * Live status pill (`GRID ONLINE` in green, `GRID DOWN` in red)
  * Live gauges / metric cards: Grid Voltage, Frequency, Inverter Load, Amperage, Power Factor, Energy
  * Inverter battery runtime estimator with dynamic autonomy bar
  * Ring-buffer table of the last 10 historical outages (start time, duration, energy consumed, pre-trip voltage, initial load)

### 5.3 Direct REST JSON Endpoints

#### 1. Instantaneous Status (`GET /api/status`)
* **Response Format:** `application/json`

```json
{
  "system": "VoltWatch",
  "uptime_seconds": 86420,
  "grid": {
    "status": "ONLINE",
    "voltage": 232.4,
    "frequency": 50.0
  },
  "inverter": {
    "load_watts": 345.2,
    "current_amps": 1.54,
    "power_factor": 0.96,
    "energy_kwh": 12.45
  },
  "battery": {
    "est_runtime_minutes": 270
  }
}
```

#### 2. Historical Outages (`GET /api/history`)
* **Response Format:** `application/json` (Array of up to 10 historical outage records)

```json
[
  {
    "start_timestamp": 1718001200,
    "end_timestamp": 1718004800,
    "duration_seconds": 3600,
    "grid_voltage_before_trip": 234.5,
    "initial_load_watts": 420.0,
    "energy_consumed_kwh": 0.420
  }
]
```

* **Integration:** Consumable by iOS Shortcuts, Home Assistant, Grafana, and CLI scripts (`curl -s http://192.168.1.17/api/status | jq`).

---

## 6. Software Stack & Toolchain Requirements

### 6.1 Toolchain & Core
* **Platform:** `arduino-cli` (v1.5.1+) on macOS (Apple Silicon arm64)
* **Core:** `esp32:esp32` (v3.3.12+ via Espressif Arduino Core)
* **Target FQBN:** `esp32:esp32:esp32` (Default ESP32 Dev Module)
* **Build System:** Monorepo root GNU `Makefile` and `firmware/Makefile`

### 6.2 Library Dependencies
| Library | Version | Purpose |
| :--- | :--- | :--- |
| PZEM004Tv30 | ^1.2.1 | Modbus RTU communication with metrology sensors |
| ESP Mail Client | ^3.4.24 | Native embedded TLS/SMTP engine |
| ArduinoJson | ^7.4.3 | JSON serialization/deserialization (v7 dynamic document API) |
| ESPAsyncWebServer | Latest | Non-blocking HTTP server for web dashboard and REST API |
| AsyncTCP | Latest | Async TCP foundation required by ESPAsyncWebServer |

---

## 7. Edge Cases & Fault Tolerance
* **PZEM Sensor Disconnection / Read Failure:**
  * If a Modbus read returns `NAN` or times out, the firmware must not crash or trigger a false alert immediately.
  * Log an error over Serial and retry up to 3 times before setting a `SENSOR_ERROR` state.
* **Wi-Fi Drop & Auto-Reconnect:**
  * Inverter room Wi-Fi signals can fluctuate.
  * Firmware runs an asynchronous background Wi-Fi health check with exponential backoff reconnect, ensuring metrology polling never blocks even when Wi-Fi is offline.
* **Outage During Boot:**
  * If the ESP32 boots up while the grid is already down, it must correctly identify the initial state as `STATE_OUTAGE` without dispatching a false "power just dropped" alert.
* **Non-Volatile Storage (NVS):**
  * Cumulative outage logs and PZEM calibration constants survive unexpected reboots via ESP32 `Preferences.h` (NVS ring buffer).
* **No Flash Wear Exhaustion:**
  * High-frequency telemetry (1 Hz) resides in RAM only.
  * Write to flash (`Preferences`) strictly on state changes (outage start, outage end) to prevent flash memory degradation.

---

## 8. Monorepo Build & Development Workflow

The repository is structured as a two-service monorepo:
* `firmware/`: Embedded C++/Arduino headless telemetry engine.
* `web/`: Standalone static web dashboard SPA.

### 8.1 Unified Root Makefile Commands
```bash
# 1. Run 100% native C++ unit tests (no hardware needed)
make test

# 2. Compile headless firmware for ESP32
make build

# 3. Upload firmware to connected ESP32 board (auto-detects port or pass PORT=...)
make flash PORT=/dev/cu.usbserial-0001 FLASH_BAUD=460800

# 4. Open serial console at 115200 baud
make monitor PORT=/dev/cu.usbserial-0001 BAUD=115200

# 5. Clean build artifacts and test binaries
make clean
```

### 8.2 Launching the Web Dashboard
```bash
# Open locally in your default web browser
open web/index.html
```
Configure your ESP32's target IP (e.g. `http://192.168.1.17`) using the settings gear in the top-right connection badge. The settings are automatically persisted to `localStorage`.
