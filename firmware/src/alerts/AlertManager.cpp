#include "AlertManager.h"

namespace VoltWatch {

AlertManager::AlertManager(const VoltageThresholds& thresholds, uint8_t debounceSamples)
    : m_thresholds(thresholds),
      m_debounceLimit(debounceSamples > 0 ? debounceSamples : 1),
      m_currentLevel(AlertLevel::NORMAL),
      m_candidateLevel(AlertLevel::NORMAL),
      m_candidateCount(0),
      m_stateChanged(false) {
}

AlertLevel AlertManager::evaluateRawLevel(float voltage) const {
    float h = m_thresholds.hysteresis;

    // Apply hysteresis based on current active level
    switch (m_currentLevel) {
        case AlertLevel::CRITICAL_OVERVOLT:
            // Remain critical high until voltage drops below threshold minus hysteresis
            if (voltage >= (m_thresholds.criticalHigh - h)) {
                return AlertLevel::CRITICAL_OVERVOLT;
            }
            break;

        case AlertLevel::WARNING_HIGH:
            if (voltage >= m_thresholds.criticalHigh) {
                return AlertLevel::CRITICAL_OVERVOLT;
            }
            if (voltage >= (m_thresholds.warningHigh - h)) {
                return AlertLevel::WARNING_HIGH;
            }
            break;

        case AlertLevel::CRITICAL_UNDERVOLT:
            // Remain critical low until voltage recovers above threshold plus hysteresis
            if (voltage <= (m_thresholds.criticalLow + h)) {
                return AlertLevel::CRITICAL_UNDERVOLT;
            }
            break;

        case AlertLevel::WARNING_LOW:
            if (voltage <= m_thresholds.criticalLow) {
                return AlertLevel::CRITICAL_UNDERVOLT;
            }
            if (voltage <= (m_thresholds.warningLow + h)) {
                return AlertLevel::WARNING_LOW;
            }
            break;

        case AlertLevel::NORMAL:
        default:
            break;
    }

    // Default threshold evaluation for standard transitions
    if (voltage >= m_thresholds.criticalHigh) {
        return AlertLevel::CRITICAL_OVERVOLT;
    }
    if (voltage >= m_thresholds.warningHigh) {
        return AlertLevel::WARNING_HIGH;
    }
    if (voltage <= m_thresholds.criticalLow) {
        return AlertLevel::CRITICAL_UNDERVOLT;
    }
    if (voltage <= m_thresholds.warningLow) {
        return AlertLevel::WARNING_LOW;
    }

    return AlertLevel::NORMAL;
}

AlertLevel AlertManager::update(float voltage) {
    m_stateChanged = false;
    AlertLevel detected = evaluateRawLevel(voltage);

    if (detected == m_currentLevel) {
        // Reset candidate debounce counter if detected matches current
        m_candidateLevel = detected;
        m_candidateCount = 0;
        return m_currentLevel;
    }

    // New candidate state detected
    if (detected == m_candidateLevel) {
        m_candidateCount++;
    } else {
        m_candidateLevel = detected;
        m_candidateCount = 1;
    }

    // Check if debounce threshold met
    if (m_candidateCount >= m_debounceLimit) {
        m_currentLevel = m_candidateLevel;
        m_candidateCount = 0;
        m_stateChanged = true;
    }

    return m_currentLevel;
}

bool AlertManager::hasStateChanged() const {
    return m_stateChanged;
}

AlertLevel AlertManager::getCurrentLevel() const {
    return m_currentLevel;
}

bool AlertManager::isCritical() const {
    return m_currentLevel == AlertLevel::CRITICAL_OVERVOLT ||
           m_currentLevel == AlertLevel::CRITICAL_UNDERVOLT;
}

bool AlertManager::isWarning() const {
    return m_currentLevel == AlertLevel::WARNING_HIGH ||
           m_currentLevel == AlertLevel::WARNING_LOW;
}

const VoltageThresholds& AlertManager::getThresholds() const {
    return m_thresholds;
}

void AlertManager::setThresholds(const VoltageThresholds& thresholds) {
    m_thresholds = thresholds;
    reset();
}

void AlertManager::reset() {
    m_currentLevel = AlertLevel::NORMAL;
    m_candidateLevel = AlertLevel::NORMAL;
    m_candidateCount = 0;
    m_stateChanged = false;
}

} // namespace VoltWatch
