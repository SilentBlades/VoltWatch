#include "StatusIndicator.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace VoltWatch {

StatusIndicator::StatusIndicator(uint8_t ledPin, uint8_t buzzerPin, bool buzzerEnabled)
    : m_ledPin(ledPin),
      m_buzzerPin(buzzerPin),
      m_buzzerEnabled(buzzerEnabled),
      m_buzzerSilenced(false),
      m_currentLevel(AlertLevel::NORMAL),
      m_lastToggleTime(0),
      m_ledState(false),
      m_buzzerState(false) {
}

void StatusIndicator::init() {
#ifdef ARDUINO
    pinMode(m_ledPin, OUTPUT);
    pinMode(m_buzzerPin, OUTPUT);
    digitalWrite(m_ledPin, LOW);
    digitalWrite(m_buzzerPin, LOW);
#endif
}

void StatusIndicator::setAlertLevel(AlertLevel level) {
    if (m_currentLevel != level) {
        m_currentLevel = level;
        m_lastToggleTime = 0; // Trigger immediate update on next cycle
    }
}

void StatusIndicator::silenceBuzzer(bool silence) {
    m_buzzerSilenced = silence;
    if (silence) {
        m_buzzerState = false;
#ifdef ARDUINO
        digitalWrite(m_buzzerPin, LOW);
#endif
    }
}

void StatusIndicator::writePins(bool ledState, bool buzzerState) {
    m_ledState = ledState;
    m_buzzerState = buzzerState && m_buzzerEnabled && !m_buzzerSilenced;

#ifdef ARDUINO
    digitalWrite(m_ledPin, m_ledState ? HIGH : LOW);
    digitalWrite(m_buzzerPin, m_buzzerState ? HIGH : LOW);
#endif
}

void StatusIndicator::update(uint32_t currentMillis) {
    switch (m_currentLevel) {
        case AlertLevel::NORMAL:
            // Heartbeat blink: 50ms pulse every 2000ms
            if (currentMillis - m_lastToggleTime >= 2000) {
                m_lastToggleTime = currentMillis;
                writePins(true, false);
            } else if (currentMillis - m_lastToggleTime >= 50 && m_ledState) {
                writePins(false, false);
            }
            break;

        case AlertLevel::WARNING_LOW:
        case AlertLevel::WARNING_HIGH:
            // Warning: steady 500ms ON / 500ms OFF toggle, no buzzer
            if (currentMillis - m_lastToggleTime >= 500) {
                m_lastToggleTime = currentMillis;
                writePins(!m_ledState, false);
            }
            break;

        case AlertLevel::CRITICAL_UNDERVOLT:
        case AlertLevel::CRITICAL_OVERVOLT:
            // Critical: rapid 100ms ON / 100ms OFF toggle with synchronized buzzer
            if (currentMillis - m_lastToggleTime >= 100) {
                m_lastToggleTime = currentMillis;
                bool nextState = !m_ledState;
                writePins(nextState, nextState);
            }
            break;
    }
}

bool StatusIndicator::isLedOn() const {
    return m_ledState;
}

bool StatusIndicator::isBuzzerOn() const {
    return m_buzzerState;
}

} // namespace VoltWatch
