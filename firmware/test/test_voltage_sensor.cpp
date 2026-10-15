#include "test_framework.h"
#include "../src/sensors/VoltageSensor.h"

TEST_CASE(VoltageSensor, ScalingCalculation) {
    // R1 = 30k, R2 = 10k -> Divider ratio = (30k + 10k) / 10k = 4.0
    // ADC 12-bit (4095), vRef = 3.3V
    VoltWatch::VoltageDividerConfig config = {
        30000.0f,
        10000.0f,
        3.3f,
        4095,
        1.0f
    };

    VoltWatch::VoltageSensor sensor(34, config);
    ASSERT_FLOAT_NEAR(4.0f, sensor.getDividerRatio(), 0.001f);

    // At full scale (ADC = 4095), Vin = 3.3V * 4.0 = 13.2V
    float fullScale = sensor.calculateVoltage(4095);
    ASSERT_FLOAT_NEAR(13.2f, fullScale, 0.01f);

    // At half scale (ADC = 2047.5), Vin = ~6.6V
    float halfScale = sensor.calculateVoltage(2047);
    ASSERT_FLOAT_NEAR(6.598f, halfScale, 0.02f);

    // At zero (ADC = 0)
    float zeroScale = sensor.calculateVoltage(0);
    ASSERT_FLOAT_NEAR(0.0f, zeroScale, 0.0001f);
}

TEST_CASE(VoltageSensor, CalibrationTrim) {
    VoltWatch::VoltageDividerConfig config = {
        30000.0f,
        10000.0f,
        3.3f,
        4095,
        1.0f
    };
    VoltWatch::VoltageSensor sensor(34, config);
    float nominal = sensor.calculateVoltage(2048);

    // Apply 5% calibration boost (1.05)
    sensor.setCalibrationFactor(1.05f);
    float calibrated = sensor.calculateVoltage(2048);
    ASSERT_FLOAT_NEAR(nominal * 1.05f, calibrated, 0.01f);
}

TEST_CASE(VoltageSensor, MockMode) {
    VoltWatch::VoltageDividerConfig config = {
        30000.0f,
        10000.0f,
        3.3f,
        4095,
        1.0f
    };
    VoltWatch::VoltageSensor sensor(34, config);
    sensor.setMockAdcValue(4095);
    ASSERT_EQ(static_cast<uint16_t>(4095), sensor.readRawAdc());
    ASSERT_FLOAT_NEAR(13.2f, sensor.readInstantaneousVoltage(), 0.01f);
}
