#ifndef VOLTWATCH_OUTAGE_STATE_MACHINE_H
#define VOLTWATCH_OUTAGE_STATE_MACHINE_H

#include <stdint.h>
#include <stddef.h>
#include <cmath>
#include "../../include/types.h"
#include "../../include/config.h"

namespace VoltWatch {

/**
 * @brief Outage state machine monitoring Grid voltage thresholds, debouncing transitions,
 *        tracking outage durations and energy deltas, and estimating battery runtime.
 */
class OutageStateMachine {
public:
    /**
     * @brief Construct an OutageStateMachine with voltage boundaries, debounce limit, and battery config.
     */
    explicit OutageStateMachine(
        float voltagePresent = VoltWatch::Config::GRID_VOLTAGE_PRESENT_V,
        float voltageOutage = VoltWatch::Config::GRID_VOLTAGE_OUTAGE_V,
        uint8_t debounceCycles = VoltWatch::Config::OUTAGE_DEBOUNCE_CYCLES,
        const BatteryConfig& batteryConfig = VoltWatch::Config::DEFAULT_BATTERY_CONFIG
    );

    /**
     * @brief Explicitly initialize the state on boot.
     *        If grid is already down, sets state to STATE_OUTAGE without firing an alert.
     */
    void init(float gridVoltage, bool gridValid, float inverterEnergy = 0.0f,
              float inverterPower = 0.0f, uint32_t currentTimestamp = 0);

    /**
     * @brief Explicitly initialize the state on boot using metric structs.
     */
    void init(const GridMetrics& grid, const InverterMetrics& inverter, uint32_t currentTimestamp = 0);

    /**
     * @brief Evaluate a polling cycle with primitive metrology values.
     * @param gridVoltage Live grid line voltage in Volts.
     * @param gridValid True if Modbus read succeeded.
     * @param inverterEnergy Inverter cumulative energy in kWh.
     * @param inverterPower Inverter active power in Watts.
     * @param currentTimestamp Current epoch or uptime timestamp in seconds.
     * @return Current committed OutageState.
     */
    OutageState update(float gridVoltage, bool gridValid, float inverterEnergy = 0.0f,
                       float inverterPower = 0.0f, uint32_t currentTimestamp = 0);

    /**
     * @brief Evaluate a polling cycle using GridMetrics and InverterMetrics structs.
     */
    OutageState update(const GridMetrics& grid, const InverterMetrics& inverter,
                       uint32_t currentTimestamp = 0);

    // State queries
    OutageState getState() const { return m_state; }
    bool hasStateChanged() const { return m_stateChanged; }
    void clearStateChanged() { m_stateChanged = false; }

    bool isNormal() const { return m_state == OutageState::STATE_NORMAL; }
    bool isOutage() const { return m_state == OutageState::STATE_OUTAGE; }
    bool isRecovered() const { return m_state == OutageState::STATE_RECOVERED; }

    bool wasBootOutage() const { return m_bootedInOutage; }
    bool isBootstrapped() const { return m_isBootstrapped; }

    OutageState getCandidateState() const { return m_candidateState; }
    uint8_t getCandidateCount() const { return m_candidateCount; }
    uint8_t getDebounceLimit() const { return m_debounceLimit; }

    // Recovery acknowledgment
    void acknowledgeRecovery();

    // Outage metrics & duration tracking
    uint32_t getActiveOutageDuration(uint32_t currentTimestamp) const;
    float getActiveOutageEnergy(float currentInverterEnergy) const;
    uint32_t getOutageStartTimestamp() const { return m_outageStartTimestamp; }
    uint32_t getOutageEndTimestamp() const { return m_outageEndTimestamp; }
    float getTripInverterEnergy() const { return m_tripInverterEnergy; }
    float getGridVoltageBeforeTrip() const { return m_gridVoltageBeforeTrip; }
    float getInitialLoadWatts() const { return m_initialLoadWatts; }

    const OutageRecord& getLastOutageRecord() const { return m_lastOutageRecord; }

    // Battery runtime estimation
    static uint32_t calculateBatteryRuntimeMinutes(const BatteryConfig& config, float loadWatts);
    uint32_t estimateBatteryRuntimeMinutes(float loadWatts) const;

    // Configuration modifiers
    void setBatteryConfig(const BatteryConfig& config) { m_batteryConfig = config; }
    const BatteryConfig& getBatteryConfig() const { return m_batteryConfig; }

    void setThresholds(float voltagePresent, float voltageOutage);
    float getVoltagePresentThreshold() const { return m_voltagePresent; }
    float getVoltageOutageThreshold() const { return m_voltageOutage; }

    void setDebounceCycles(uint8_t cycles) { m_debounceLimit = cycles > 0 ? cycles : 1; }

    void reset();

private:
    void bootstrap(float gridVoltage, bool gridValid, float inverterEnergy,
                   float inverterPower, uint32_t currentTimestamp);

    float m_voltagePresent;
    float m_voltageOutage;
    uint8_t m_debounceLimit;
    BatteryConfig m_batteryConfig;

    OutageState m_state{OutageState::STATE_NORMAL};
    OutageState m_candidateState{OutageState::STATE_NORMAL};
    uint8_t m_candidateCount{0};

    bool m_stateChanged{false};
    bool m_isBootstrapped{false};
    bool m_bootedInOutage{false};

    uint32_t m_outageStartTimestamp{0};
    uint32_t m_outageEndTimestamp{0};
    float m_tripInverterEnergy{0.0f};
    float m_restoreInverterEnergy{0.0f};
    float m_gridVoltageBeforeTrip{0.0f};
    float m_initialLoadWatts{0.0f};
    float m_lastGoodGridVoltage{230.0f};

    OutageRecord m_lastOutageRecord{0, 0, 0, 0.0f, 0.0f, 0.0f};
};

} // namespace VoltWatch

#endif // VOLTWATCH_OUTAGE_STATE_MACHINE_H
