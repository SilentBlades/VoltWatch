#ifndef VOLTWATCH_VOLTAGE_SENSOR_H
#define VOLTWATCH_VOLTAGE_SENSOR_H

#include <stdint.h>
#include "../../include/types.h"

namespace VoltWatch {

/**
 * @brief Voltage sensor abstraction handling ADC conversion and resistor divider scaling.
 */
class VoltageSensor {
public:
    /**
     * @brief Construct a VoltageSensor instance.
     * @param pin Analog input GPIO pin.
     * @param divider Resistor divider configuration.
     */
    VoltageSensor(uint8_t pin, const VoltageDividerConfig& divider);

    /**
     * @brief Initialize hardware ADC configuration.
     */
    void init();

    /**
     * @brief Read raw digital ADC value from hardware (or mock).
     * @return 12-bit ADC value (0-4095).
     */
    uint16_t readRawAdc();

    /**
     * @brief Convert an arbitrary ADC count to measured input voltage.
     * @param adcValue Raw ADC digital count.
     * @return Converted real-world voltage in Volts.
     */
    float calculateVoltage(uint16_t adcValue) const;

    /**
     * @brief Read the current instantaneous input voltage.
     * @return Instantaneous measured voltage in Volts.
     */
    float readInstantaneousVoltage();

    /**
     * @brief Get the computed voltage divider ratio (Vin / Vpin).
     */
    float getDividerRatio() const;

    /**
     * @brief Update the fine-tuning calibration factor.
     * @param factor Multiplier correction factor (nominal 1.0).
     */
    void setCalibrationFactor(float factor);

    /**
     * @brief Update the resistor configuration.
     */
    void setDividerConfig(const VoltageDividerConfig& config);

    /**
     * @brief Inject a mock ADC reading for testing or simulation.
     */
    void setMockAdcValue(uint16_t mockValue);

    /**
     * @brief Enable or disable mock ADC mode.
     */
    void setMockEnabled(bool enabled);

    /**
     * @brief Get the configured analog pin number.
     */
    uint8_t getPin() const { return m_pin; }

private:
    uint8_t m_pin;
    VoltageDividerConfig m_config;
    uint16_t m_mockValue{0};
    bool m_mockEnabled{false};
};

} // namespace VoltWatch

#endif // VOLTWATCH_VOLTAGE_SENSOR_H
