#ifndef VOLTWATCH_STATUS_INDICATOR_H
#define VOLTWATCH_STATUS_INDICATOR_H

#include <stdint.h>
#include "../../include/types.h"

namespace VoltWatch {

/**
 * @brief Non-blocking LED and buzzer indicator manager.
 */
class StatusIndicator {
public:
    StatusIndicator(uint8_t ledPin, uint8_t buzzerPin, bool buzzerEnabled = true);

    /**
     * @brief Configure GPIO output modes.
     */
    void init();

    /**
     * @brief Set current alert level to adjust blink and alarm patterns.
     */
    void setAlertLevel(AlertLevel level);

    /**
     * @brief Non-blocking tick called inside loop() to handle blinking patterns.
     * @param currentMillis Current timestamp in milliseconds.
     */
    void update(uint32_t currentMillis);

    /**
     * @brief Silence buzzer audio alarm.
     */
    void silenceBuzzer(bool silence);

    /**
     * @brief Get current LED state (for testing/diagnostics).
     */
    bool isLedOn() const;

    /**
     * @brief Get current buzzer state.
     */
    bool isBuzzerOn() const;

    /**
     * @brief Get configured LED GPIO pin.
     */
    uint8_t getLedPin() const { return m_ledPin; }

    /**
     * @brief Get configured Buzzer GPIO pin.
     */
    uint8_t getBuzzerPin() const { return m_buzzerPin; }

private:
    void writePins(bool ledState, bool buzzerState);

    uint8_t m_ledPin;
    uint8_t m_buzzerPin;
    bool m_buzzerEnabled;
    bool m_buzzerSilenced{false};

    AlertLevel m_currentLevel{AlertLevel::NORMAL};
    uint32_t m_lastToggleTime{0};
    bool m_ledState{false};
    bool m_buzzerState{false};
};

} // namespace VoltWatch

#endif // VOLTWATCH_STATUS_INDICATOR_H
