#include <Arduino.h>
#include <ESP_Mail_Client.h>

#if defined(ARDUINO) || defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <time.h>
#include <LittleFS.h>
#endif

#if __has_include(<PZEM004Tv30.h>)
#include <PZEM004Tv30.h>
#endif

#include "include/config.h"
#include "include/pins.h"
#include "include/types.h"
#include "src/metrology/PZEMManager.h"
#include "src/statemachine/OutageStateMachine.h"
#include "src/alerts/SMTPAlertDispatcher.h"
#include "src/storage/OutageStorage.h"
#include "src/network/WiFiManager.h"
#include "src/web/WebDashboard.h"
#include "src/display/StatusIndicator.h"

// =============================================================================
// Static Component Instantiations
// =============================================================================

// Dedicated Dual HardwareSerial PZEM-004T v3.0 Metrology Manager:
// - Serial1 on GPIO 18/19 for Grid Mains (Address 0x01)
// - Serial2 on GPIO 16/17 for Inverter Output (Address 0x02 or 0x01)
// Dual dedicated UART buses eliminate factory address conflicts and optocoupler phototransistor bus contention.
static VoltWatch::PZEMManager g_pzem(
    VoltWatch::Pins::PZEM_GRID_RX_PIN,
    VoltWatch::Pins::PZEM_GRID_TX_PIN,
    VoltWatch::Pins::PZEM_INV_RX_PIN,
    VoltWatch::Pins::PZEM_INV_TX_PIN,
    VoltWatch::Config::PZEM_GRID_ADDR,
    VoltWatch::Config::PZEM_INVERTER_ADDR,
    VoltWatch::Config::METROLOGY_POLL_INTERVAL_MS,
    VoltWatch::Config::PZEM_MAX_RETRIES,
    VoltWatch::Config::PZEM_OUTAGE_POLL_INTERVAL_MS,
    VoltWatch::Config::PZEM_OUTAGE_MAX_RETRIES
);

// Outage State Machine with 3-cycle debouncing & realistic battery autonomy estimator
static VoltWatch::OutageStateMachine g_stateMachine(
    VoltWatch::Config::GRID_VOLTAGE_PRESENT_V,
    VoltWatch::Config::GRID_VOLTAGE_OUTAGE_V,
    VoltWatch::Config::OUTAGE_DEBOUNCE_CYCLES,
    VoltWatch::Config::DEFAULT_BATTERY_CONFIG
);

// Direct SMTPS Alert Dispatcher:
// Runs on a dedicated FreeRTOS background task pinned to Core 0 with offline FIFO alert queue.
// loop() on Core 1 pushes via FreeRTOS queue with 0ms delay!
static VoltWatch::SMTPAlertDispatcher g_alertDispatcher;

// Outage History Ring-Buffer Storage in NVS with Flash Wear Protection
static VoltWatch::OutageStorage g_storage;

// Non-blocking Asynchronous Wi-Fi Manager with Exponential Backoff
static VoltWatch::WiFiManager g_wifi;

// Embedded Asynchronous Web Server, REST API (GET /api/status) & SSE pipeline
static VoltWatch::WebDashboard g_webDashboard;

// Non-blocking Status Indicator (LED & Buzzer)
static VoltWatch::StatusIndicator g_indicator(
    VoltWatch::Pins::STATUS_LED_PIN,
    VoltWatch::Pins::ALERT_BUZZER_PIN,
    VoltWatch::Config::BUZZER_ENABLED
);

// Interactive Bench Simulation Flags (allows testing full outage lifecycle via Serial Monitor)
// Default to false on boot: boots directly into live physical PZEM hardware acquisition mode
bool g_simActive = false;
bool g_simOutage = false;

// Sync local NVS records into web dashboard cache helper
static void syncWebHistory() {
    VoltWatch::OutageRecord records[VoltWatch::OutageStorage::MAX_CAPACITY];
    size_t count = g_storage.count();
    for (size_t i = 0; i < count; ++i) {
        g_storage.getRecord(i, records[i]);
    }
    g_webDashboard.updateHistory(records, count);
}

// Non-blocking 1 Hz metrology throttling timer
static uint32_t g_lastMetrologyPoll = 0;

void setup() {
    Serial.begin(VoltWatch::Config::SERIAL_BAUD_RATE);
    delay(500); // Allow power rail and UART to stabilize

#if defined(ARDUINO) && defined(ESP32)
    // Configure FreeRTOS Task Watchdog Timer to 10,000 ms with panic trigger
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 10000,
        .idle_core_mask = 0, // Do NOT monitor idle tasks; only monitor registered tasks
        .trigger_panic = true
    };
    esp_task_wdt_reconfigure(&twdt_config);
    esp_task_wdt_add(NULL); // Subscribe main loopTask to TWDT
#endif

    Serial.println();
    Serial.println("===============================================================");
    Serial.println("   VoltWatch: Zero-Server Dual-UART Metrology Sentinel        ");
    Serial.println("===============================================================");
    Serial.println("Architecture: Standalone Edge-Computed Metrology Platform");
    Serial.printf("Grid Metrology:     Hardware Serial1 (RX1: GPIO %u, TX1: GPIO %u)\n",
                  VoltWatch::Pins::PZEM_GRID_RX_PIN, VoltWatch::Pins::PZEM_GRID_TX_PIN);
    Serial.printf("Inverter Metrology: Hardware Serial2 (RX2: GPIO %u, TX2: GPIO %u)\n",
                  VoltWatch::Pins::PZEM_INV_RX_PIN, VoltWatch::Pins::PZEM_INV_TX_PIN);
    Serial.printf("Thresholds:         Grid Present >= %.1fV | Grid Outage < %.1fV\n",
                  VoltWatch::Config::GRID_VOLTAGE_PRESENT_V, VoltWatch::Config::GRID_VOLTAGE_OUTAGE_V);
    Serial.printf("Debounce Window:    %u consecutive cycles (%u ms)\n",
                  VoltWatch::Config::OUTAGE_DEBOUNCE_CYCLES,
                  VoltWatch::Config::OUTAGE_DEBOUNCE_CYCLES * VoltWatch::Config::METROLOGY_POLL_INTERVAL_MS);
    Serial.printf("Outage Throttling:  Grid polling throttled to %u ms (0 retries) during outages\n",
                  VoltWatch::Config::PZEM_OUTAGE_POLL_INTERVAL_MS);
    Serial.printf("SMTP Worker:        FreeRTOS Task pinned to Core %u (%u bytes stack)\n",
                  VoltWatch::Config::SMTP_TASK_CORE, VoltWatch::Config::SMTP_TASK_STACK_SIZE);
    Serial.printf("Dashboard URL:      http://%s.local / http://<IP>/\n", VoltWatch::Config::MDNS_HOSTNAME);
    Serial.println("===============================================================");

    // 1. Initialize hardware peripherals & indicators
    g_indicator.init();

#if defined(ARDUINO) || defined(ESP32)
    // 1b. Initialize LittleFS with formatOnFail = true (prevents ESP_Mail_Client mount error -84)
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] ⚠️ LittleFS mount failed even after formatting.");
    } else {
        Serial.println("[FS] ✅ LittleFS initialized and mounted successfully.");
    }
#endif

    // 2. Initialize persistent NVS storage (ring-buffer of 10 historical outages)
    g_storage.begin();
    Serial.printf("[NVS] Loaded %zu historical outage records from flash.\n", g_storage.count());

    // 3. Initialize dual dedicated PZEM metrology hardware serial buses
    g_pzem.init();

    // 4. Initialize asynchronous Wi-Fi manager (non-blocking)
    Serial.printf("[WiFi] Starting non-blocking connection to SSID: '%s'...\n", WIFI_SSID);
    g_wifi.init(WIFI_SSID, WIFI_PASSWORD);

    // 5. Initialize SMTPS alerting engine, FreeRTOS Core 0 worker task, and NTP synchronization
    g_alertDispatcher.init();
    g_alertDispatcher.initNtp();

    // 6. Configure asynchronous Web Server routes (Socket listening deferred until Wi-Fi connects)
    g_webDashboard.begin(80);
    syncWebHistory();

    // 7. Initial metrology reading to bootstrap state machine
    Serial.println("[Init] Performing initial metrology acquisition...");
    g_pzem.setSimulationMode(g_simActive, g_simOutage);
    g_pzem.forcePoll();
    g_lastMetrologyPoll = millis();
    const VoltWatch::GridMetrics& initialGrid = g_pzem.getGridMetrics();
    const VoltWatch::InverterMetrics& initialInv = g_pzem.getInverterMetrics();

    // Evaluate initial power state (handles outage during boot without false alarm)
    g_stateMachine.init(initialGrid, initialInv, 0);

    if (g_stateMachine.wasBootOutage()) {
        Serial.println("[State] ⚠️ Grid OFFLINE on boot! Running on battery/inverter.");
        Serial.println("[State] Boot outage handled: State set to STATE_OUTAGE without false alarm.");
        g_indicator.setAlertLevel(VoltWatch::AlertLevel::WARNING_LOW);
        g_pzem.setOutageMode(true);
    } else {
        Serial.printf("[State] ✅ Grid ONLINE on boot: %.1fV @ %.1fHz. Inverter Load: %.1fW.\n",
                      initialGrid.voltage, initialGrid.frequency, initialInv.power);
        g_indicator.setAlertLevel(VoltWatch::AlertLevel::NORMAL);
        g_pzem.setOutageMode(false);
    }

    Serial.println("VoltWatch operational. Starting real-time metrology loop...");
    if (g_simActive) {
        Serial.println("[Mode] 🧪 Bench Simulation Mode active (physical PZEM UART reads disabled).");
        Serial.println("[Tip] Type 'hardware' or 'live' in Serial Monitor to enable physical PZEM sensors.");
        Serial.println("[Tip] Type 'outage' to simulate blackout. Type 'help' for console commands.\n");
    } else {
        Serial.println("[Mode] ⚡ Live Hardware Acquisition active (Dual PZEM-004T v3.0 UARTs).");
        Serial.println("[Tip] Type 'sim' or 'outage' in Serial Monitor for interactive bench testing.\n");
    }
}

static String g_serialCmdBuffer = "";

static void handleSerialConsole() {
    int maxBytes = 32; // Limit processing to 32 bytes per tick to prevent starvation under heavy stream
    while (Serial.available() > 0 && maxBytes-- > 0) {
        char c = static_cast<char>(Serial.read());
        if (c == '\r' || c == '\n') {
            g_serialCmdBuffer.trim();
            if (g_serialCmdBuffer.length() > 0) {
                String cmd = g_serialCmdBuffer;
                cmd.toLowerCase();
                g_serialCmdBuffer = "";

                if (cmd == "outage" || cmd == "trip") {
                    g_simActive = true;
                    g_simOutage = true;
                    Serial.println("\n[Sim] ⚡ Simulation: GRID OUTAGE triggered! (Grid V forced to 0.0V)");
                    Serial.println("[Sim] Debouncing 3 cycles (3000ms)... Alert will dispatch to FreeRTOS SMTP worker.");
                } else if (cmd == "restore" || cmd == "recover") {
                    g_simActive = true;
                    g_simOutage = false;
                    Serial.println("\n[Sim] ⚡ Simulation: GRID RESTORED! (Grid V forced to 231.5V)");
                    Serial.println("[Sim] Debouncing 3 cycles (3000ms)... Restoration alert will dispatch.");
                } else if (cmd == "sim") {
                    g_simActive = true;
                    Serial.println("\n[Sim] Bench simulation mode enabled. Physical UART reads bypassed.");
                } else if (cmd == "live" || cmd == "normal" || cmd == "hardware") {
                    g_simActive = false;
                    Serial.println("\n[Sim] Simulation disabled. Resuming live PZEM hardware reads.");
                } else if (cmd == "status") {
                    Serial.printf("[Status] Wi-Fi: %s | State: %s | Mode: %s\n",
                                  g_wifi.isConnected() ? "CONNECTED" : "CONNECTING/DISCONNECTED",
                                  VoltWatch::outageStateToString(g_stateMachine.getState()),
                                  g_simActive ? (g_simOutage ? "SIM_OUTAGE (0V)" : "SIM_RESTORE (231.5V)") : "LIVE_PZEM");
                } else if (cmd == "help") {
                    Serial.println("\n--- VoltWatch Bench Simulation Console ---");
                    Serial.println("  sim               - Enable bench simulation mode (bypasses physical UART reads)");
                    Serial.println("  outage / trip     - Drop grid voltage to 0V (Simulates blackout -> triggers email alert)");
                    Serial.println("  restore / recover - Restore grid voltage to 231.5V (Simulates recovery -> triggers email)");
                    Serial.println("  hardware / live   - Resume reading physical PZEM hardware sensors");
                    Serial.println("  status            - Print live network, power state, and simulation status");
                    Serial.println("------------------------------------------\n");
                } else {
                    Serial.printf("\n[Console] Unknown command: '%s'. Type 'help' for available commands.\n", cmd.c_str());
                }
            }
        } else if (c >= 32 && c <= 126) {
            if (g_serialCmdBuffer.length() < 64) {
                g_serialCmdBuffer += c;
            }
        }
    }
}

void loop() {
#if defined(ARDUINO) && defined(ESP32)
    esp_task_wdt_reset();
#endif
    yield();

    uint32_t currentMillis = millis();
    uint32_t currentUptimeSec = currentMillis / 1000;


    // -------------------------------------------------------------------------
    // Task 0: Status Indicator Cadence Tick (Executed FIRST so LED blinks reliably)
    // -------------------------------------------------------------------------
    g_indicator.update(currentMillis);

    // -------------------------------------------------------------------------
    // Task 1: Interactive Serial Simulation Console (Executed FIRST for instant response)
    // -------------------------------------------------------------------------
    handleSerialConsole();

    // -------------------------------------------------------------------------
    // Task 2: Non-Blocking Asynchronous Wi-Fi State Machine Tick
    // -------------------------------------------------------------------------
    g_wifi.update(currentMillis);

    // When Wi-Fi connects & acquires an IP:
    // 1. Trigger NTP time sync (UTC+5:30 IST) required for SMTPS TLS certificate validation
    // 2. Start WebDashboard listening on port 80
    static bool s_ntpSyncedOnConnect = false;
#if defined(ARDUINO) && !defined(UNIT_TEST)
    if (WiFi.status() == WL_CONNECTED) {
        if (!s_ntpSyncedOnConnect) {
            s_ntpSyncedOnConnect = true;
            Serial.printf("[NTP] Wi-Fi connected (IP: %s)! Synchronizing system time for UTC+5:30 (IST)...\n",
                          WiFi.localIP().toString().c_str());
            configTime(19800, 0, "pool.ntp.org", "time.google.com"); // UTC+5:30 (IST)
            g_alertDispatcher.initNtp();
        }
        if (!g_webDashboard.isRunning()) {
            g_webDashboard.startListening();
        }
    } else {
        s_ntpSyncedOnConnect = false;
    }
#else
    if (g_wifi.isConnected() && !g_webDashboard.isRunning()) {
        g_webDashboard.startListening();
    }
#endif

    // -------------------------------------------------------------------------
    // Task 2: 1 Hz Metrology Polling & Outage State Evaluation
    // Strictly throttled to run once every 1000 ms using a non-blocking millis() timer
    // -------------------------------------------------------------------------
    if (currentMillis - g_lastMetrologyPoll >= VoltWatch::Config::METROLOGY_POLL_INTERVAL_MS) {
        g_lastMetrologyPoll = currentMillis;

        VoltWatch::GridMetrics grid{};
        VoltWatch::InverterMetrics inverter{};

        if (g_simActive) {
            // Bench Simulation: Skip physical UART reads entirely so offline bench testing never blocks on Modbus timeouts
            if (g_simOutage) {
                grid.voltage = 0.0f;
                grid.frequency = 0.0f;
                grid.isValid = true;
                grid.readErrors = 0;
            } else {
                grid.voltage = 231.5f;
                grid.frequency = 50.0f;
                grid.isValid = true;
                grid.readErrors = 0;
            }

            inverter.voltage = 230.0f;
            inverter.current = 1.45f;
            inverter.power = 320.0f; // 320W typical load
            inverter.energy = 1.25f;
            inverter.frequency = 50.0f;
            inverter.powerFactor = 0.96f;
            inverter.isValid = true;
            inverter.readErrors = 0;
        } else {
            // Live Hardware: Synchronize outage polling throttling (in an outage, grid polling is throttled to 3s with 0 retries)
            g_pzem.setOutageMode(g_stateMachine.isOutage());

            g_pzem.poll(currentMillis);
            grid = g_pzem.getGridMetrics();
            inverter = g_pzem.getInverterMetrics();
        }

        // Update outage state machine
        VoltWatch::OutageState currentState = g_stateMachine.update(grid, inverter, currentUptimeSec);

        // State Transition Handling
        if (g_stateMachine.hasStateChanged()) {
            if (currentState == VoltWatch::OutageState::STATE_OUTAGE) {
                Serial.printf("\n🚨 [ALERT] Grid Outage Committed! Uptime: %us | Pre-Trip: %.1fV | Load: %.1fW\n",
                              currentUptimeSec,
                              g_stateMachine.getGridVoltageBeforeTrip(),
                              g_stateMachine.getInitialLoadWatts());

                // Flash wear protection: record outage start strictly on state transition
                g_storage.recordOutageStart(
                    currentUptimeSec,
                    g_stateMachine.getGridVoltageBeforeTrip(),
                    g_stateMachine.getInitialLoadWatts()
                );

                // Prepare and queue direct SMTPS Outage Alert (FreeRTOS Core 0 task dispatches asynchronously)
                VoltWatch::OutageAlertParams alertParams;
                time_t nowOutage = time(nullptr);
                uint32_t outageEpoch = (nowOutage >= 1700000000) ? static_cast<uint32_t>(nowOutage) : currentUptimeSec;
                alertParams.timestampEpoch = outageEpoch;
                alertParams.gridVoltageBeforeTrip = g_stateMachine.getGridVoltageBeforeTrip();
                alertParams.loadWatts = inverter.power;
                alertParams.currentAmps = inverter.current;
                alertParams.powerFactor = inverter.powerFactor;
                alertParams.estBatteryRuntimeMin = g_stateMachine.estimateBatteryRuntimeMinutes(inverter.power);
                alertParams.isNtpSynced = VoltWatch::SMTPAlertDispatcher::isNtpSynced(outageEpoch);

                g_alertDispatcher.sendOutageAlert(alertParams);
                g_indicator.setAlertLevel(VoltWatch::AlertLevel::CRITICAL_UNDERVOLT);

            } else if (currentState == VoltWatch::OutageState::STATE_RECOVERED) {
                const VoltWatch::OutageRecord& lastOutage = g_stateMachine.getLastOutageRecord();

                Serial.printf("\n✅ [RECOVERED] Grid Restored! Duration: %us | Consumed: %.3f kWh | Grid: %.1fV\n",
                              lastOutage.durationSeconds,
                              lastOutage.energyConsumedKwh,
                              grid.voltage);

                // Flash wear protection: record outage end strictly on state transition
                g_storage.recordOutageEnd(currentUptimeSec, lastOutage.energyConsumedKwh);

                // Prepare and queue direct SMTPS Restoration Alert (flushes queued outage alert first if router was down)
                VoltWatch::RestorationAlertParams restoreParams;
                time_t nowRestore = time(nullptr);
                uint32_t restoreEpoch = (nowRestore >= 1700000000) ? static_cast<uint32_t>(nowRestore) : currentUptimeSec;
                restoreParams.timestampEpoch = restoreEpoch;
                restoreParams.durationSeconds = lastOutage.durationSeconds;
                restoreParams.energyConsumedKwh = lastOutage.energyConsumedKwh;
                restoreParams.restoredGridVoltage = grid.voltage;
                restoreParams.restoredGridFrequency = grid.frequency;
                restoreParams.isNtpSynced = VoltWatch::SMTPAlertDispatcher::isNtpSynced(restoreEpoch);

                g_alertDispatcher.sendRestorationAlert(restoreParams);

                // Acknowledge recovery and reset state to NORMAL
                g_stateMachine.acknowledgeRecovery();
                g_indicator.setAlertLevel(VoltWatch::AlertLevel::NORMAL);
            }

            // Sync updated outage history into WebDashboard
            syncWebHistory();
        }

        // Build aggregate PowerSystemStatus
        VoltWatch::PowerSystemStatus status;
        status.uptimeSeconds = currentUptimeSec;
        status.state = currentState;
        status.grid = grid;
        status.inverter = inverter;
        status.estBatteryRuntimeMinutes = g_stateMachine.estimateBatteryRuntimeMinutes(inverter.power);
        status.activeOutageDurationSec = g_stateMachine.getActiveOutageDuration(currentUptimeSec);
        status.activeOutageEnergyKwh = g_stateMachine.getActiveOutageEnergy(inverter.energy);

        // Update WebDashboard telemetry caches
        g_webDashboard.updateStatus(status);
    }

    // -------------------------------------------------------------------------
    // Task 3: Web Server Client Request Handling (Non-blocking handleClient)
    // -------------------------------------------------------------------------
    if (g_webDashboard.isRunning()) {
        g_webDashboard.update(currentMillis);
    }

    // -------------------------------------------------------------------------
    // Task 4: FreeRTOS Task Yield (Unconditional 50ms yield on every pass)
    // Prevents IDLE task starvation, feeds Task Watchdog Timer (TWDT), and avoids panic
    // -------------------------------------------------------------------------
#if defined(ARDUINO) || defined(ESP32)
    vTaskDelay(pdMS_TO_TICKS(50));
#endif
}
