#include "OutageStateMachine.h"

namespace VoltWatch {

OutageStateMachine::OutageStateMachine(float voltagePresent,
                                       float voltageOutage,
                                       uint8_t debounceCycles,
                                       const BatteryConfig& batteryConfig)
    : m_voltagePresent(voltagePresent),
      m_voltageOutage(voltageOutage),
      m_debounceLimit(debounceCycles > 0 ? debounceCycles : 1),
      m_batteryConfig(batteryConfig) {
}

void OutageStateMachine::init(float gridVoltage, bool gridValid, float inverterEnergy,
                              float inverterPower, uint32_t currentTimestamp) {
    bootstrap(gridVoltage, gridValid, inverterEnergy, inverterPower, currentTimestamp);
}

void OutageStateMachine::init(const GridMetrics& grid, const InverterMetrics& inverter,
                              uint32_t currentTimestamp) {
    bootstrap(grid.voltage, grid.isValid, inverter.energy, inverter.power, currentTimestamp);
}

void OutageStateMachine::bootstrap(float gridVoltage, bool gridValid, float inverterEnergy,
                                   float inverterPower, uint32_t currentTimestamp) {
    bool isDown = (!gridValid || std::isnan(gridVoltage) || gridVoltage < m_voltageOutage);

    if (isDown) {
        m_state = OutageState::STATE_OUTAGE;
        m_candidateState = OutageState::STATE_OUTAGE;
        m_candidateCount = 0;
        m_stateChanged = false; // Do NOT fire a false outage alert on boot
        m_bootedInOutage = true;
        m_isBootstrapped = true;

        m_outageStartTimestamp = currentTimestamp;
        m_outageEndTimestamp = 0;
        m_tripInverterEnergy = inverterEnergy;
        m_initialLoadWatts = inverterPower;
        m_gridVoltageBeforeTrip = 0.0f;
    } else {
        m_state = OutageState::STATE_NORMAL;
        m_candidateState = OutageState::STATE_NORMAL;
        m_candidateCount = 0;
        m_stateChanged = false;
        m_bootedInOutage = false;
        m_isBootstrapped = true;
        m_lastGoodGridVoltage = gridVoltage;
    }
}

OutageState OutageStateMachine::update(const GridMetrics& grid, const InverterMetrics& inverter,
                                       uint32_t currentTimestamp) {
    return update(grid.voltage, grid.isValid, inverter.energy, inverter.power, currentTimestamp);
}

OutageState OutageStateMachine::update(float gridVoltage, bool gridValid, float inverterEnergy,
                                       float inverterPower, uint32_t currentTimestamp) {
    if (!m_isBootstrapped) {
        bootstrap(gridVoltage, gridValid, inverterEnergy, inverterPower, currentTimestamp);
        return m_state;
    }

    m_stateChanged = false;

    bool isRawOutage = (!gridValid || std::isnan(gridVoltage) || gridVoltage < m_voltageOutage);
    bool isRawPresent = (gridValid && !std::isnan(gridVoltage) && gridVoltage >= m_voltagePresent);

    OutageState targetState = m_state;

    if (isRawPresent) {
        m_lastGoodGridVoltage = gridVoltage;
        if (m_state == OutageState::STATE_OUTAGE) {
            targetState = OutageState::STATE_RECOVERED;
        } else {
            targetState = m_state;
        }
    } else if (isRawOutage) {
        targetState = OutageState::STATE_OUTAGE;
    } else {
        // Indeterminate hysteresis band: maintain current active state
        targetState = m_state;
    }

    if (targetState == m_state) {
        m_candidateState = m_state;
        m_candidateCount = 0;
        return m_state;
    }

    if (targetState == m_candidateState) {
        m_candidateCount++;
    } else {
        m_candidateState = targetState;
        m_candidateCount = 1;
    }

    if (m_candidateCount >= m_debounceLimit) {
        m_state = m_candidateState;
        m_candidateCount = 0;
        m_stateChanged = true;

        if (m_state == OutageState::STATE_OUTAGE) {
            m_outageStartTimestamp = currentTimestamp;
            m_outageEndTimestamp = 0;
            m_tripInverterEnergy = inverterEnergy;
            m_initialLoadWatts = inverterPower;
            m_gridVoltageBeforeTrip = m_lastGoodGridVoltage;
        } else if (m_state == OutageState::STATE_RECOVERED) {
            m_outageEndTimestamp = currentTimestamp;
            m_restoreInverterEnergy = inverterEnergy;
            uint32_t dur = (m_outageEndTimestamp >= m_outageStartTimestamp)
                           ? (m_outageEndTimestamp - m_outageStartTimestamp) : 0;
            float energyDelta = (m_restoreInverterEnergy >= m_tripInverterEnergy)
                                ? (m_restoreInverterEnergy - m_tripInverterEnergy) : 0.0f;
            bool isSynced = (m_outageStartTimestamp > 1700000000 && m_outageEndTimestamp > 1700000000);
            m_lastOutageRecord = OutageRecord{
                m_outageStartTimestamp,
                m_outageEndTimestamp,
                dur,
                m_gridVoltageBeforeTrip,
                m_initialLoadWatts,
                energyDelta,
                isSynced
            };
        }
    }

    return m_state;
}

void OutageStateMachine::acknowledgeRecovery() {
    if (m_state == OutageState::STATE_RECOVERED) {
        m_state = OutageState::STATE_NORMAL;
        m_candidateState = OutageState::STATE_NORMAL;
        m_candidateCount = 0;
        m_stateChanged = false;
    }
}

uint32_t OutageStateMachine::getActiveOutageDuration(uint32_t currentTimestamp) const {
    if (m_state != OutageState::STATE_OUTAGE) {
        return 0;
    }
    if (currentTimestamp >= m_outageStartTimestamp) {
        return currentTimestamp - m_outageStartTimestamp;
    }
    return 0;
}

float OutageStateMachine::getActiveOutageEnergy(float currentInverterEnergy) const {
    if (m_state != OutageState::STATE_OUTAGE) {
        return 0.0f;
    }
    if (currentInverterEnergy >= m_tripInverterEnergy) {
        return currentInverterEnergy - m_tripInverterEnergy;
    }
    return 0.0f;
}

uint32_t OutageStateMachine::calculateBatteryRuntimeMinutes(const BatteryConfig& config, float loadWatts) {
    if (loadWatts < 0.0f || config.batteryVoltage <= 0.0f || config.capacityAh <= 0.0f ||
        config.inverterEfficiency <= 0.0f || config.maxDepthOfDischarge <= 0.0f) {
        return 0;
    }

    // Effective load accounts for inverter parasitic tare loss and minimum load floor
    float effectiveLoad = loadWatts + config.tareLossWatts;
    if (config.minEffectiveLoadWatts > 0.0f && effectiveLoad < config.minEffectiveLoadWatts) {
        effectiveLoad = config.minEffectiveLoadWatts;
    }

    if (effectiveLoad <= 0.0f) {
        return 0;
    }

    float usableWh = config.batteryVoltage * config.capacityAh *
                     config.maxDepthOfDischarge * config.inverterEfficiency;
    float minutes = (usableWh * 60.0f) / effectiveLoad;

    if (minutes <= 0.0f || std::isnan(minutes) || std::isinf(minutes)) {
        return 0;
    }

    uint32_t result = static_cast<uint32_t>(minutes + 0.5f);
    if (config.maxAutonomyMinutes > 0 && result > config.maxAutonomyMinutes) {
        result = config.maxAutonomyMinutes;
    }
    return result;
}

uint32_t OutageStateMachine::estimateBatteryRuntimeMinutes(float loadWatts) const {
    return calculateBatteryRuntimeMinutes(m_batteryConfig, loadWatts);
}

void OutageStateMachine::setThresholds(float voltagePresent, float voltageOutage) {
    m_voltagePresent = voltagePresent;
    m_voltageOutage = voltageOutage;
}

void OutageStateMachine::reset() {
    m_state = OutageState::STATE_NORMAL;
    m_candidateState = OutageState::STATE_NORMAL;
    m_candidateCount = 0;
    m_stateChanged = false;
    m_isBootstrapped = false;
    m_bootedInOutage = false;
    m_outageStartTimestamp = 0;
    m_outageEndTimestamp = 0;
    m_tripInverterEnergy = 0.0f;
    m_restoreInverterEnergy = 0.0f;
    m_gridVoltageBeforeTrip = 0.0f;
    m_initialLoadWatts = 0.0f;
    m_lastGoodGridVoltage = 230.0f;
    m_lastOutageRecord = OutageRecord{0, 0, 0, 0.0f, 0.0f, 0.0f};
}

} // namespace VoltWatch
