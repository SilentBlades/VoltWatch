#ifndef VOLTWATCH_PZEM_MANAGER_H
#define VOLTWATCH_PZEM_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <cmath>
#include "../../include/types.h"
#include "../../include/config.h"
#include "../../include/pins.h"

namespace VoltWatch {

/**
 * @brief Raw metrology readings container from a PZEM-004T module read.
 */
struct PZEMRawReading {
    float voltage{NAN};
    float current{NAN};
    float power{NAN};
    float energy{NAN};
    float frequency{NAN};
    float powerFactor{NAN};
};

/**
 * @brief Hardware abstraction interface for PZEM-004T Modbus-RTU communications.
 */
class IPZEMReader {
public:
    virtual ~IPZEMReader() = default;

    /**
     * @brief Read raw metrics from a PZEM module at the specified Modbus address.
     * @param addr Modbus RTU address (0x01 for Grid, 0x02 for Inverter).
     * @param reading Output struct populated with telemetry readings.
     * @return True if transport read was executed; false if serial timeout/transport error.
     */
    virtual bool readRaw(uint8_t addr, PZEMRawReading& reading) = 0;
};

/**
 * @brief Mock implementation of IPZEMReader for unit tests and simulation.
 */
class MockPZEMReader : public IPZEMReader {
public:
    MockPZEMReader();

    void setGridReading(float voltage, float frequency, bool isValid = true);
    void setInverterReading(float voltage, float current, float power, float energy,
                            float frequency, float powerFactor, bool isValid = true);

    void setGridFailures(uint8_t count, bool returnNan = false);
    void setInverterFailures(uint8_t count, bool returnNan = false);

    uint32_t getGridReadCount() const { return m_gridReadCount; }
    uint32_t getInverterReadCount() const { return m_inverterReadCount; }

    void resetCounts() {
        m_gridReadCount = 0;
        m_inverterReadCount = 0;
    }

    bool readRaw(uint8_t addr, PZEMRawReading& reading) override;

private:
    PZEMRawReading m_gridData;
    PZEMRawReading m_inverterData;
    bool m_gridValid{true};
    bool m_inverterValid{true};
    uint8_t m_gridFailuresRemaining{0};
    uint8_t m_inverterFailuresRemaining{0};
    bool m_gridFailAsNan{false};
    bool m_inverterFailAsNan{false};
    uint32_t m_gridReadCount{0};
    uint32_t m_inverterReadCount{0};
};

/**
 * @brief Dual PZEM-004T v3.0 Manager for sequential Grid & Inverter metrology acquisition.
 *
 * Supports two distinct dedicated HardwareSerial UART instances on ESP32:
 * - Serial1 on GPIO 18/19 for Grid Mains (Address 0x01)
 * - Serial2 on GPIO 16/17 for Inverter Output (Address 0x02 or 0x01)
 * This eliminates the factory address trap (0x01 contention) and optocoupler phototransistor bus clamping.
 *
 * Includes Outage Throttling: When the Grid is known to be in an outage, Grid polling is throttled
 * (e.g. to 3000 ms with 0 retries) so timeout latency does not starve Inverter metrology or stall SSE telemetry.
 */
class PZEMManager {
public:
    /**
     * @brief Construct PZEMManager with dedicated dual UART pins or single-bus fallbacks.
     */
    PZEMManager(
        uint8_t gridRxPin,
        uint8_t gridTxPin,
        uint8_t invRxPin,
        uint8_t invTxPin,
        uint8_t gridAddr = VoltWatch::Config::PZEM_GRID_ADDR,
        uint8_t inverterAddr = VoltWatch::Config::PZEM_INVERTER_ADDR,
        uint32_t pollIntervalMs = VoltWatch::Config::METROLOGY_POLL_INTERVAL_MS,
        uint8_t maxRetries = VoltWatch::Config::PZEM_MAX_RETRIES,
        uint32_t outagePollIntervalMs = VoltWatch::Config::PZEM_OUTAGE_POLL_INTERVAL_MS,
        uint8_t outageMaxRetries = VoltWatch::Config::PZEM_OUTAGE_MAX_RETRIES
    );

    /**
     * @brief Backward-compatible 2-pin constructor (maps to dual UART defaults).
     */
    explicit PZEMManager(
        uint8_t rxPin = VoltWatch::Pins::PZEM_INV_RX_PIN,
        uint8_t txPin = VoltWatch::Pins::PZEM_INV_TX_PIN,
        uint8_t gridAddr = VoltWatch::Config::PZEM_GRID_ADDR,
        uint8_t inverterAddr = VoltWatch::Config::PZEM_INVERTER_ADDR,
        uint32_t pollIntervalMs = VoltWatch::Config::METROLOGY_POLL_INTERVAL_MS,
        uint8_t maxRetries = VoltWatch::Config::PZEM_MAX_RETRIES
    );

    ~PZEMManager();

    /**
     * @brief Initialize hardware serial buses (Serial1 and Serial2 on ESP32) or mock delegates.
     */
    void init();

    /**
     * @brief Periodic polling handler. Executes sequential read if interval elapsed.
     *        Honors outage throttling when inOutageMode is set.
     * @param currentMillis Current system timestamp in milliseconds.
     * @return True if a polling cycle was executed; false if skipped (interval not met).
     */
    bool poll(uint32_t currentMillis = 0);

    /**
     * @brief Force an immediate sequential poll regardless of elapsed interval.
     */
    void forcePoll();

    /**
     * @brief Execute a sequential read of the Grid module with retries.
     * @return True if read was valid, false if retries exhausted.
     */
    bool readGrid();

    /**
     * @brief Execute a sequential read of the Inverter module with retries.
     * @return True if read was valid, false if retries exhausted.
     */
    bool readInverter();

    // --- Outage Polling Throttling Controls ---
    void setOutageMode(bool inOutage) { m_inOutageMode = inOutage; }
    bool isOutageMode() const { return m_inOutageMode; }

    void setOutagePollInterval(uint32_t intervalMs) { m_outagePollIntervalMs = intervalMs; }
    uint32_t getOutagePollInterval() const { return m_outagePollIntervalMs; }

    void setOutageMaxRetries(uint8_t retries) { m_outageMaxRetries = retries; }
    uint8_t getOutageMaxRetries() const { return m_outageMaxRetries; }

    // Metric accessors
    const GridMetrics& getGridMetrics() const { return m_gridMetrics; }
    const InverterMetrics& getInverterMetrics() const { return m_inverterMetrics; }

    bool isGridValid() const { return m_gridMetrics.isValid; }
    bool isInverterValid() const { return m_inverterMetrics.isValid; }

    uint8_t getGridReadErrors() const { return m_gridMetrics.readErrors; }
    uint8_t getInverterReadErrors() const { return m_inverterMetrics.readErrors; }

    uint8_t getLastGridRetries() const { return m_lastGridRetries; }
    uint8_t getLastInverterRetries() const { return m_lastInverterRetries; }

    uint32_t getLastPollTime() const { return m_lastPollTime; }
    uint32_t getLastGridPollTime() const { return m_lastGridPollTime; }
    uint32_t getTotalPolls() const { return m_totalPolls; }

    uint8_t getGridRxPin() const { return m_gridRxPin; }
    uint8_t getGridTxPin() const { return m_gridTxPin; }
    uint8_t getInvRxPin() const { return m_invRxPin; }
    uint8_t getInvTxPin() const { return m_invTxPin; }
    uint8_t getRxPin() const { return m_invRxPin; }
    uint8_t getTxPin() const { return m_invTxPin; }

    uint8_t getGridAddr() const { return m_gridAddr; }
    uint8_t getInverterAddr() const { return m_inverterAddr; }

    void setPollInterval(uint32_t intervalMs) { m_pollIntervalMs = intervalMs; }
    uint32_t getPollInterval() const { return m_pollIntervalMs; }

    void setMaxRetries(uint8_t maxRetries) { m_maxRetries = maxRetries; }
    uint8_t getMaxRetries() const { return m_maxRetries; }

    void setLoggingEnabled(bool enabled) { m_loggingEnabled = enabled; }
    bool isLoggingEnabled() const { return m_loggingEnabled; }

    // Mock and test delegation
    void setCustomReader(IPZEMReader* reader);
    MockPZEMReader* getMockReader() { return &m_defaultMockReader; }
    void setMockGridReading(float voltage, float frequency, bool isValid = true);
    void setMockInverterReading(float voltage, float current, float power, float energy,
                                float frequency, float powerFactor, bool isValid = true);
    void setMockGridFailures(uint8_t count, bool returnNan = false);
    void setMockInverterFailures(uint8_t count, bool returnNan = false);

    // Simulation controls (bypasses physical UART reads entirely)
    void setSimulationMode(bool enabled, bool outage = false) {
        m_simulationMode = enabled;
        m_simOutage = outage;
    }
    bool isSimulationMode() const { return m_simulationMode; }
    void setSimulationOutage(bool outage) { m_simOutage = outage; }
    bool isSimulationOutage() const { return m_simOutage; }

    void reset();

private:
    bool m_simulationMode{false};
    bool m_simOutage{false};
    void logWarning(uint8_t addr, uint8_t attempt, uint8_t maxRetries);
    void logError(uint8_t addr, uint8_t maxRetries);

    uint8_t m_gridRxPin;
    uint8_t m_gridTxPin;
    uint8_t m_invRxPin;
    uint8_t m_invTxPin;

    uint8_t m_gridAddr;
    uint8_t m_inverterAddr;
    uint32_t m_pollIntervalMs;
    uint8_t m_maxRetries;

    // Outage throttling parameters
    bool m_inOutageMode{false};
    uint32_t m_outagePollIntervalMs;
    uint8_t m_outageMaxRetries;
    uint32_t m_lastGridPollTime{0};

    uint32_t m_lastPollTime{0};
    uint32_t m_totalPolls{0};
    uint8_t m_lastGridRetries{0};
    uint8_t m_lastInverterRetries{0};
    bool m_loggingEnabled{false};

    GridMetrics m_gridMetrics{0.0f, 0.0f, false, 0};
    InverterMetrics m_inverterMetrics{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false, 0};

    MockPZEMReader m_defaultMockReader;
    IPZEMReader* m_reader{&m_defaultMockReader};
    bool m_ownsReader{false};
};

} // namespace VoltWatch

#endif // VOLTWATCH_PZEM_MANAGER_H
