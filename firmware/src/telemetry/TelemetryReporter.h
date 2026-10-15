#ifndef VOLTWATCH_TELEMETRY_REPORTER_H
#define VOLTWATCH_TELEMETRY_REPORTER_H

#include <stddef.h>
#include <stdint.h>
#include "../../include/types.h"

namespace VoltWatch {

/**
 * @brief Handles formatting and serial streaming of telemetry and alert packets.
 */
class TelemetryReporter {
public:
    TelemetryReporter() = default;

    /**
     * @brief Serialize a voltage reading into a JSON-formatted string buffer.
     * @param reading VoltageReading structure.
     * @param outBuffer Destination character buffer.
     * @param bufferSize Size of destination buffer.
     * @return Number of characters written (excluding null terminator).
     */
    static size_t formatJson(const VoltageReading& reading, char* outBuffer, size_t bufferSize);

    /**
     * @brief Format an alert state change event into a JSON string buffer.
     */
    static size_t formatAlertJson(AlertLevel oldLevel, AlertLevel newLevel, float voltage,
                                  uint32_t timestampMs, char* outBuffer, size_t bufferSize);

    /**
     * @brief Stream reading JSON packet over Arduino Serial (if ARDUINO defined).
     */
    static void printSerial(const VoltageReading& reading);

    /**
     * @brief Stream alert event JSON over Arduino Serial (if ARDUINO defined).
     */
    static void printAlertSerial(AlertLevel oldLevel, AlertLevel newLevel, float voltage, uint32_t timestampMs);
};

} // namespace VoltWatch

#endif // VOLTWATCH_TELEMETRY_REPORTER_H
