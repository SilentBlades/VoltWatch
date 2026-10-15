#ifndef VOLTWATCH_CONFIG_H
#define VOLTWATCH_CONFIG_H

#include "types.h"
#include "pins.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

namespace VoltWatch {
namespace Config {

    // --- Serial Communication ---
    constexpr uint32_t SERIAL_BAUD_RATE      = 115200; // Debug Console Baud Rate
    constexpr uint32_t PZEM_BAUD_RATE        = 9600;   // PZEM-004T v3.0 Modbus RTU Baud Rate

    // --- PZEM Modbus Addresses ---
    constexpr uint8_t PZEM_GRID_ADDR         = 0x01;   // Grid Mains Metrology Module
    constexpr uint8_t PZEM_INVERTER_ADDR     = 0x02;   // Inverter Output Metrology Module
    constexpr uint8_t PZEM_MAX_RETRIES       = 3;      // Consecutive retries before SENSOR_ERROR

    // --- Metrology & State Machine Timing (milliseconds) ---
    constexpr uint32_t METROLOGY_POLL_INTERVAL_MS = 1000; // 1 Hz normal polling cycle
    constexpr uint32_t TELEMETRY_INTERVAL_MS      = 1000; // 1 Hz telemetry push
    constexpr uint32_t SSE_PUSH_INTERVAL_MS       = 1000; // 1 Hz SSE web update

    // --- Outage Polling Throttling (Preserves 1-second budget during blackouts) ---
    constexpr uint32_t PZEM_OUTAGE_POLL_INTERVAL_MS = 3000; // Back off Grid polling to 3s when Grid is down
    constexpr uint8_t  PZEM_OUTAGE_MAX_RETRIES      = 0;    // Zero retries when Grid is known to be dead

    // --- Grid Outage & Restoration Thresholds ---
    constexpr float GRID_VOLTAGE_PRESENT_V   = 170.0f; // Voltage >= 170.0V indicates Grid Normal
    constexpr float GRID_VOLTAGE_OUTAGE_V    = 80.0f;  // Voltage < 80.0V indicates Grid Outage
    constexpr uint8_t OUTAGE_DEBOUNCE_CYCLES = 3;      // 3 consecutive cycles (3,000 ms) to commit

    // --- Outage History Storage ---
    constexpr size_t OUTAGE_HISTORY_MAX      = 10;     // Last 10 historical outages in NVS ring-buffer

    // --- Battery Backup & Runtime Estimation Parameters ---
    #ifndef BATTERY_VOLTAGE
    #define BATTERY_VOLTAGE 12.0f      // Nominal battery bank voltage (12V)
    #endif
    #ifndef BATTERY_CAPACITY_AH
    #define BATTERY_CAPACITY_AH 150.0f // Battery capacity: 150 Ah (1.8 kWh nominal)
    #endif
    #ifndef INVERTER_EFFICIENCY
    #define INVERTER_EFFICIENCY 0.85f  // DC->AC inverter efficiency (85%)
    #endif
    #ifndef BATTERY_MAX_DOD
    #define BATTERY_MAX_DOD 0.80f      // Max safe Depth of Discharge (80% for tubular/lead-acid)
    #endif

    constexpr BatteryConfig DEFAULT_BATTERY_CONFIG = {
        BATTERY_VOLTAGE,
        BATTERY_CAPACITY_AH,
        INVERTER_EFFICIENCY,
        BATTERY_MAX_DOD,
        20.0f,  // Tare loss (W): Inverter idle parasitic draw
        25.0f,  // Min effective load floor (W): Prevents divide-by-zero & absurd estimates
        2880    // Autonomy ceiling (minutes): Max 48 hours reported
    };

    // --- FreeRTOS SMTP Task & Offline Alert Queue Settings ---
    constexpr uint32_t SMTP_TASK_STACK_SIZE  = 16384;  // 16KB stack for mbedTLS / SMTPS
    constexpr uint8_t  SMTP_TASK_CORE        = 0;      // Pin to Core 0 (leaving Core 1 for real-time loop)
    constexpr uint8_t  SMTP_TASK_PRIORITY    = 1;      // Background priority
    constexpr size_t   SMTP_QUEUE_CAPACITY   = 8;      // Offline FIFO pending alert queue capacity

    // --- Local mDNS Hostname ---
    constexpr char MDNS_HOSTNAME[] = "voltwatch"; // http://voltwatch.local

    // --- SMTPS Server Transport Defaults ---
    #ifndef SMTP_HOST
    #define SMTP_HOST "smtp.gmail.com"
    #endif
    #ifndef SMTP_PORT
    #define SMTP_PORT 465 // SMTPS over SSL/TLS
    #endif

    // =========================================================================
    // CREDENTIAL FALLBACKS (Used ONLY when include/secrets.h is absent)
    // -------------------------------------------------------------------------
    // All actual credentials (WIFI_SSID, WIFI_PASSWORD, SMTP_SENDER_EMAIL,
    // SMTP_APP_PASSWORD, SMTP_RECIPIENT_1/2/3) must be configured in:
    //   include/secrets.h
    //
    // The #ifndef blocks below are fallback stubs to prevent compilation errors
    // during automated CI builds or tests when secrets.h has not been created.
    // =========================================================================
    #ifndef WIFI_SSID
    #define WIFI_SSID ""
    #endif
    #ifndef WIFI_PASSWORD
    #define WIFI_PASSWORD ""
    #endif
    #ifndef SMTP_SENDER_NAME
    #define SMTP_SENDER_NAME "VoltWatch Sentinel"
    #endif
    #ifndef SMTP_SENDER_EMAIL
    #define SMTP_SENDER_EMAIL ""
    #endif
    #ifndef SMTP_APP_PASSWORD
    #define SMTP_APP_PASSWORD ""
    #endif
    #ifndef SMTP_RECIPIENT_1
    #define SMTP_RECIPIENT_1 ""
    #endif
    #ifndef SMTP_RECIPIENT_2
    #define SMTP_RECIPIENT_2 ""
    #endif
    #ifndef SMTP_RECIPIENT_3
    #define SMTP_RECIPIENT_3 ""
    #endif

    // --- NTP Time Synchronization ---
    constexpr char NTP_SERVER[]              = "pool.ntp.org";
    constexpr long TIMEZONE_OFFSET_SEC       = 19800;  // IST (UTC+5:30 = 5.5 * 3600 = 19800s)
    constexpr int  DAYLIGHT_OFFSET_SEC       = 0;      // India has no DST

    // --- Legacy DC Sensor & Divider Config (retained for backward compatibility) ---
    constexpr uint32_t SAMPLE_INTERVAL_MS    = 100;
    constexpr size_t FILTER_WINDOW_SIZE      = 16;
    constexpr VoltageDividerConfig DEFAULT_DIVIDER = {
        30000.0f, 7500.0f, 3.30f, 4095, 1.00f
    };
    constexpr VoltageThresholds DEFAULT_THRESHOLDS = {
        12.0f, 10.5f, 11.4f, 12.8f, 14.4f, 0.20f
    };
    constexpr uint8_t ALERT_DEBOUNCE_SAMPLES = 3;
    constexpr bool BUZZER_ENABLED            = true;

} // namespace Config
} // namespace VoltWatch

#endif // VOLTWATCH_CONFIG_H
