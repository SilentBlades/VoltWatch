#include "TelemetryReporter.h"
#include <stdio.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace VoltWatch {

size_t TelemetryReporter::formatJson(const VoltageReading& reading, char* outBuffer, size_t bufferSize) {
    if (!outBuffer || bufferSize == 0) {
        return 0;
    }

    int written = snprintf(
        outBuffer,
        bufferSize,
        "{\"type\":\"telemetry\",\"uptime_ms\":%u,\"v_raw\":%.3f,\"v_avg\":%.3f,\"adc\":%u,\"status\":\"%s\"}",
        reading.timestampMs,
        reading.rawVoltage,
        reading.filteredVoltage,
        reading.rawAdcValue,
        alertLevelToString(reading.alertLevel)
    );

    if (written < 0) {
        outBuffer[0] = '\0';
        return 0;
    }

    if (static_cast<size_t>(written) >= bufferSize) {
        return bufferSize - 1;
    }

    return static_cast<size_t>(written);
}

size_t TelemetryReporter::formatAlertJson(AlertLevel oldLevel, AlertLevel newLevel, float voltage,
                                         uint32_t timestampMs, char* outBuffer, size_t bufferSize) {
    if (!outBuffer || bufferSize == 0) {
        return 0;
    }

    int written = snprintf(
        outBuffer,
        bufferSize,
        "{\"type\":\"alert\",\"uptime_ms\":%u,\"voltage\":%.3f,\"old_status\":\"%s\",\"new_status\":\"%s\"}",
        timestampMs,
        voltage,
        alertLevelToString(oldLevel),
        alertLevelToString(newLevel)
    );

    if (written < 0) {
        outBuffer[0] = '\0';
        return 0;
    }

    if (static_cast<size_t>(written) >= bufferSize) {
        return bufferSize - 1;
    }

    return static_cast<size_t>(written);
}

void TelemetryReporter::printSerial(const VoltageReading& reading) {
#ifdef ARDUINO
    char buffer[160];
    formatJson(reading, buffer, sizeof(buffer));
    Serial.println(buffer);
#else
    char buffer[160];
    formatJson(reading, buffer, sizeof(buffer));
    printf("%s\n", buffer);
#endif
}

void TelemetryReporter::printAlertSerial(AlertLevel oldLevel, AlertLevel newLevel, float voltage, uint32_t timestampMs) {
#ifdef ARDUINO
    char buffer[160];
    formatAlertJson(oldLevel, newLevel, voltage, timestampMs, buffer, sizeof(buffer));
    Serial.println(buffer);
#else
    char buffer[160];
    formatAlertJson(oldLevel, newLevel, voltage, timestampMs, buffer, sizeof(buffer));
    printf("%s\n", buffer);
#endif
}

} // namespace VoltWatch
