#include "test_framework.h"
#include "../src/telemetry/TelemetryReporter.h"
#include <cstring>

TEST_CASE(Telemetry, JsonFormatting) {
    VoltWatch::VoltageReading reading = {
        12.345f,
        12.300f,
        3050,
        15000,
        VoltWatch::AlertLevel::NORMAL
    };

    char buffer[256];
    size_t len = VoltWatch::TelemetryReporter::formatJson(reading, buffer, sizeof(buffer));

    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strstr(buffer, "\"type\":\"telemetry\"") != nullptr);
    ASSERT_TRUE(strstr(buffer, "\"uptime_ms\":15000") != nullptr);
    ASSERT_TRUE(strstr(buffer, "\"status\":\"NORMAL\"") != nullptr);
    ASSERT_TRUE(strstr(buffer, "\"adc\":3050") != nullptr);
}

TEST_CASE(Telemetry, AlertJsonFormatting) {
    char buffer[256];
    size_t len = VoltWatch::TelemetryReporter::formatAlertJson(
        VoltWatch::AlertLevel::NORMAL,
        VoltWatch::AlertLevel::CRITICAL_OVERVOLT,
        14.850f,
        25000,
        buffer,
        sizeof(buffer)
    );

    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strstr(buffer, "\"type\":\"alert\"") != nullptr);
    ASSERT_TRUE(strstr(buffer, "\"old_status\":\"NORMAL\"") != nullptr);
    ASSERT_TRUE(strstr(buffer, "\"new_status\":\"CRITICAL_OVERVOLT\"") != nullptr);
    ASSERT_TRUE(strstr(buffer, "\"uptime_ms\":25000") != nullptr);
}
