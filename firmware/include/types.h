#ifndef VOLTWATCH_TYPES_H
#define VOLTWATCH_TYPES_H

#include <stdint.h>
#include <stddef.h>

namespace VoltWatch {

/**
 * @brief System power and grid outage operational states
 */
enum class OutageState : uint8_t {
    STATE_NORMAL = 0,    ///< Grid is healthy (>= 170V) and supplying loads
    STATE_OUTAGE,        ///< Grid dropped below threshold (< 80V); running on inverter/battery
    STATE_RECOVERED      ///< Grid restored above operational threshold after an outage
};

inline const char* outageStateToString(OutageState state) {
    switch (state) {
        case OutageState::STATE_NORMAL:    return "STATE_NORMAL";
        case OutageState::STATE_OUTAGE:    return "STATE_OUTAGE";
        case OutageState::STATE_RECOVERED: return "STATE_RECOVERED";
        default:                           return "UNKNOWN";
    }
}

inline const char* gridStatusString(OutageState state) {
    switch (state) {
        case OutageState::STATE_NORMAL:
        case OutageState::STATE_RECOVERED: return "ONLINE";
        case OutageState::STATE_OUTAGE:    return "OFFLINE";
        default:                           return "UNKNOWN";
    }
}

/**
 * @brief Legacy alert level retained for indicator LED/Buzzer backward compatibility
 */
enum class AlertLevel : uint8_t {
    NORMAL = 0,
    WARNING_LOW,
    WARNING_HIGH,
    CRITICAL_UNDERVOLT,
    CRITICAL_OVERVOLT
};

inline const char* alertLevelToString(AlertLevel level) {
    switch (level) {
        case AlertLevel::NORMAL:             return "NORMAL";
        case AlertLevel::WARNING_LOW:        return "WARNING_LOW";
        case AlertLevel::WARNING_HIGH:       return "WARNING_HIGH";
        case AlertLevel::CRITICAL_UNDERVOLT: return "CRITICAL_UNDERVOLT";
        case AlertLevel::CRITICAL_OVERVOLT:  return "CRITICAL_OVERVOLT";
        default:                             return "UNKNOWN";
    }
}

/**
 * @brief Metrology readings from Grid Mains PZEM-004T module (Address 0x01)
 */
struct GridMetrics {
    float voltage;       ///< Grid AC Line Voltage (V)
    float frequency;     ///< Grid AC Line Frequency (Hz)
    bool isValid;        ///< True if last Modbus read was successful
    uint8_t readErrors;  ///< Consecutive read error count
};

/**
 * @brief Metrology readings from Inverter Output PZEM-004T module (Address 0x02)
 */
struct InverterMetrics {
    float voltage;       ///< Inverter Output AC Voltage (V)
    float current;       ///< Inverter Load Current (A)
    float power;         ///< Inverter Active Power (W)
    float energy;        ///< Cumulative Energy consumed (kWh)
    float frequency;     ///< Output Frequency (Hz)
    float powerFactor;   ///< Inverter Power Factor (0.00 - 1.00)
    bool isValid;        ///< True if last Modbus read was successful
    uint8_t readErrors;  ///< Consecutive read error count
};

/**
 * @brief Historical outage record stored in NVS ring-buffer
 */
struct OutageRecord {
    uint32_t startTimestamp;       ///< Epoch timestamp (or uptime seconds) when outage started
    uint32_t endTimestamp;         ///< Epoch timestamp when grid recovered (0 if ongoing)
    uint32_t durationSeconds;      ///< Total outage duration in seconds
    float gridVoltageBeforeTrip;   ///< Last recorded grid voltage prior to tripping
    float initialLoadWatts;        ///< Inverter active load (W) when outage started
    float energyConsumedKwh;       ///< Battery energy consumed during outage (kWh)
    bool isNtpSynced{false};       ///< True if timestamps are real epoch; false if uptime seconds
};

/**
 * @brief Battery runtime estimator parameters
 */
struct BatteryConfig {
    float batteryVoltage;               ///< Nominal battery bank voltage (e.g., 12.0V or 24.0V)
    float capacityAh;                   ///< Total battery bank capacity in Amp-hours (e.g., 150Ah)
    float inverterEfficiency;           ///< DC-to-AC conversion efficiency (e.g., 0.85 = 85%)
    float maxDepthOfDischarge;          ///< Maximum safe DoD fraction (e.g., 0.80 = 80%)
    float tareLossWatts{0.0f};          ///< Inverter no-load parasitic tare loss (W)
    float minEffectiveLoadWatts{0.0f};  ///< Minimum effective load floor (prevents div-by-zero)
    uint32_t maxAutonomyMinutes{0};     ///< Maximum reported autonomy ceiling (0 = unconstrained)
};

/**
 * @brief Aggregated real-time system power status
 */
struct PowerSystemStatus {
    uint32_t uptimeSeconds;             ///< Microcontroller uptime in seconds
    OutageState state;                  ///< Current grid outage state
    GridMetrics grid;                   ///< Live grid mains metrology
    InverterMetrics inverter;           ///< Live inverter output metrology
    uint32_t estBatteryRuntimeMinutes;  ///< Estimated remaining battery backup runtime in minutes
    uint32_t activeOutageDurationSec;   ///< Duration of currently active outage (0 if normal)
    float activeOutageEnergyKwh;        ///< Energy consumed since current outage started
};

/**
 * @brief Legacy VoltageReading & thresholds for analog divider tests compatibility
 */
struct VoltageReading {
    float rawVoltage;
    float filteredVoltage;
    uint16_t rawAdcValue;
    uint32_t timestampMs;
    AlertLevel alertLevel;
};

struct VoltageThresholds {
    float nominalVoltage;
    float criticalLow;
    float warningLow;
    float warningHigh;
    float criticalHigh;
    float hysteresis;
};

struct VoltageDividerConfig {
    float r1Ohms;
    float r2Ohms;
    float vRef;
    uint16_t adcResolution;
    float calibrationFactor;
};

} // namespace VoltWatch

#endif // VOLTWATCH_TYPES_H
