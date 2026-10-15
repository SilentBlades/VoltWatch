#ifndef VOLTWATCH_ALERT_MANAGER_H
#define VOLTWATCH_ALERT_MANAGER_H

#include <stdint.h>
#include "../../include/types.h"

namespace VoltWatch {

/**
 * @brief Manages voltage threshold monitoring with hysteresis and sample debouncing.
 */
class AlertManager {
public:
    /**
     * @brief Construct an AlertManager with configured thresholds and debounce count.
     * @param thresholds Voltage threshold boundaries.
     * @param debounceSamples Number of consecutive readings needed to confirm a state change.
     */
    explicit AlertManager(const VoltageThresholds& thresholds, uint8_t debounceSamples = 3);

    /**
     * @brief Evaluate a new filtered voltage reading.
     * @param voltage Current filtered voltage.
     * @return Confirmed current AlertLevel.
     */
    AlertLevel update(float voltage);

    /**
     * @brief Check whether the alert state changed in the most recent update() call.
     */
    bool hasStateChanged() const;

    /**
     * @brief Get the currently active alert level.
     */
    AlertLevel getCurrentLevel() const;

    /**
     * @brief Check if the current state is any critical condition.
     */
    bool isCritical() const;

    /**
     * @brief Check if the current state is any warning condition.
     */
    bool isWarning() const;

    /**
     * @brief Get configured thresholds.
     */
    const VoltageThresholds& getThresholds() const;

    /**
     * @brief Update threshold configurations at runtime.
     */
    void setThresholds(const VoltageThresholds& thresholds);

    /**
     * @brief Reset manager state to NORMAL.
     */
    void reset();

private:
    AlertLevel evaluateRawLevel(float voltage) const;

    VoltageThresholds m_thresholds;
    uint8_t m_debounceLimit;

    AlertLevel m_currentLevel{AlertLevel::NORMAL};
    AlertLevel m_candidateLevel{AlertLevel::NORMAL};
    uint8_t m_candidateCount{0};
    bool m_stateChanged{false};
};

} // namespace VoltWatch

#endif // VOLTWATCH_ALERT_MANAGER_H
