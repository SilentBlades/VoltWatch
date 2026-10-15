#include "PZEMManager.h"

#if defined(ARDUINO)
#include <Arduino.h>
#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
#if __has_include(<PZEM004Tv30.h>)
#include <PZEM004Tv30.h>
#define VOLTWATCH_HAS_PZEM_LIB 1
#endif
#else
#include <cstdio>
#endif

namespace VoltWatch {

#if defined(ARDUINO) && defined(VOLTWATCH_HAS_PZEM_LIB)
/**
 * @brief Dedicated Dual Hardware UART PZEM Reader on ESP32.
 * Serial1 is assigned to Grid PZEM; Serial2 is assigned to Inverter PZEM.
 * This completely isolates the buses and avoids optocoupler clamping and address collisions.
 */
class HardwarePZEMReader : public IPZEMReader {
public:
    HardwarePZEMReader(uint8_t gridRx, uint8_t gridTx, uint8_t invRx, uint8_t invTx,
                       uint8_t gridAddr, uint8_t invAddr)
        : m_gridRx(gridRx), m_gridTx(gridTx), m_invRx(invRx), m_invTx(invTx),
          m_gridAddr(gridAddr), m_invAddr(invAddr),
          m_pzemGrid(Serial1, gridRx, gridTx, gridAddr),
          m_pzemInv(Serial2, invRx, invTx, invAddr) {}

    void init() {
        Serial1.begin(VoltWatch::Config::PZEM_BAUD_RATE, SERIAL_8N1, m_gridRx, m_gridTx);
        Serial1.setTimeout(100);
        Serial2.begin(VoltWatch::Config::PZEM_BAUD_RATE, SERIAL_8N1, m_invRx, m_invTx);
        Serial2.setTimeout(100);
    }

    bool readRaw(uint8_t addr, PZEMRawReading& reading) override {
        PZEM004Tv30* pzem = nullptr;
        HardwareSerial* port = nullptr;
        if (addr == m_gridAddr) {
            pzem = &m_pzemGrid;
            port = &Serial1;
        } else if (addr == m_invAddr) {
            pzem = &m_pzemInv;
            port = &Serial2;
        } else {
            return false;
        }

        port->setTimeout(100);

        reading.voltage = pzem->voltage();
        if (std::isnan(reading.voltage)) {
            return false; // Fast exit on timeout without reading 5 more registers
        }

        reading.current = pzem->current();
        reading.power = pzem->power();
        reading.energy = pzem->energy();
        reading.frequency = pzem->frequency();
        reading.powerFactor = pzem->pf();

        return true;
    }

private:
    uint8_t m_gridRx;
    uint8_t m_gridTx;
    uint8_t m_invRx;
    uint8_t m_invTx;
    uint8_t m_gridAddr;
    uint8_t m_invAddr;
    PZEM004Tv30 m_pzemGrid;
    PZEM004Tv30 m_pzemInv;
};
#endif

// =============================================================================
// MockPZEMReader Implementation
// =============================================================================

MockPZEMReader::MockPZEMReader() {
    m_gridData.voltage = 230.0f;
    m_gridData.frequency = 50.0f;

    m_inverterData.voltage = 230.0f;
    m_inverterData.current = 1.5f;
    m_inverterData.power = 345.0f;
    m_inverterData.energy = 10.0f;
    m_inverterData.frequency = 50.0f;
    m_inverterData.powerFactor = 0.95f;
}

void MockPZEMReader::setGridReading(float voltage, float frequency, bool isValid) {
    m_gridValid = isValid;
    m_gridData.voltage = voltage;
    m_gridData.frequency = frequency;
}

void MockPZEMReader::setInverterReading(float voltage, float current, float power, float energy,
                                        float frequency, float powerFactor, bool isValid) {
    m_inverterValid = isValid;
    m_inverterData.voltage = voltage;
    m_inverterData.current = current;
    m_inverterData.power = power;
    m_inverterData.energy = energy;
    m_inverterData.frequency = frequency;
    m_inverterData.powerFactor = powerFactor;
}

void MockPZEMReader::setGridFailures(uint8_t count, bool returnNan) {
    m_gridFailuresRemaining = count;
    m_gridFailAsNan = returnNan;
}

void MockPZEMReader::setInverterFailures(uint8_t count, bool returnNan) {
    m_inverterFailuresRemaining = count;
    m_inverterFailAsNan = returnNan;
}

bool MockPZEMReader::readRaw(uint8_t addr, PZEMRawReading& reading) {
    if (addr == VoltWatch::Config::PZEM_GRID_ADDR) {
        m_gridReadCount++;
        if (m_gridFailuresRemaining > 0) {
            m_gridFailuresRemaining--;
            if (m_gridFailAsNan) {
                reading.voltage = NAN;
                reading.frequency = NAN;
                return true;
            }
            return false;
        }

        if (!m_gridValid) {
            return false;
        }

        reading = m_gridData;
        return true;
    } else if (addr == VoltWatch::Config::PZEM_INVERTER_ADDR) {
        m_inverterReadCount++;
        if (m_inverterFailuresRemaining > 0) {
            m_inverterFailuresRemaining--;
            if (m_inverterFailAsNan) {
                reading.voltage = NAN;
                reading.current = NAN;
                reading.power = NAN;
                reading.energy = NAN;
                reading.frequency = NAN;
                reading.powerFactor = NAN;
                return true;
            }
            return false;
        }

        if (!m_inverterValid) {
            return false;
        }

        reading = m_inverterData;
        return true;
    }

    return false;
}

// =============================================================================
// PZEMManager Implementation
// =============================================================================

PZEMManager::PZEMManager(
    uint8_t gridRxPin,
    uint8_t gridTxPin,
    uint8_t invRxPin,
    uint8_t invTxPin,
    uint8_t gridAddr,
    uint8_t inverterAddr,
    uint32_t pollIntervalMs,
    uint8_t maxRetries,
    uint32_t outagePollIntervalMs,
    uint8_t outageMaxRetries
) : m_gridRxPin(gridRxPin),
    m_gridTxPin(gridTxPin),
    m_invRxPin(invRxPin),
    m_invTxPin(invTxPin),
    m_gridAddr(gridAddr),
    m_inverterAddr(inverterAddr),
    m_pollIntervalMs(pollIntervalMs),
    m_maxRetries(maxRetries),
    m_inOutageMode(false),
    m_outagePollIntervalMs(outagePollIntervalMs),
    m_outageMaxRetries(outageMaxRetries) {
}

PZEMManager::PZEMManager(
    uint8_t rxPin,
    uint8_t txPin,
    uint8_t gridAddr,
    uint8_t inverterAddr,
    uint32_t pollIntervalMs,
    uint8_t maxRetries
) : PZEMManager(
        VoltWatch::Pins::PZEM_GRID_RX_PIN,
        VoltWatch::Pins::PZEM_GRID_TX_PIN,
        rxPin,
        txPin,
        gridAddr,
        inverterAddr,
        pollIntervalMs,
        maxRetries,
        VoltWatch::Config::PZEM_OUTAGE_POLL_INTERVAL_MS,
        VoltWatch::Config::PZEM_OUTAGE_MAX_RETRIES
    ) {
}

PZEMManager::~PZEMManager() {
    if (m_ownsReader && m_reader != nullptr) {
        delete m_reader;
        m_reader = nullptr;
    }
}

void PZEMManager::init() {
#if defined(ARDUINO) && defined(VOLTWATCH_HAS_PZEM_LIB)
    if (m_ownsReader && m_reader != nullptr) {
        delete m_reader;
        m_reader = nullptr;
    }
    auto* hwReader = new HardwarePZEMReader(
        m_gridRxPin, m_gridTxPin, m_invRxPin, m_invTxPin, m_gridAddr, m_inverterAddr
    );
    hwReader->init();
    m_reader = hwReader;
    m_ownsReader = true;
#else
    m_reader = &m_defaultMockReader;
    m_ownsReader = false;
#endif
}

#if defined(ARDUINO)
extern bool g_simActive __attribute__((weak));
extern bool g_simOutage __attribute__((weak));
#endif

bool PZEMManager::poll(uint32_t currentMillis) {
    bool simActive = m_simulationMode;
#if defined(ARDUINO)
    if (&g_simActive != nullptr && g_simActive) {
        simActive = true;
    }
#endif

    if (simActive) {
        if (currentMillis > 0 && m_lastPollTime > 0) {
            if (currentMillis - m_lastPollTime < m_pollIntervalMs) {
                return false;
            }
        }
        m_lastPollTime = currentMillis;
        m_totalPolls++;

        bool outage = m_simOutage;
#if defined(ARDUINO)
        if (&g_simOutage != nullptr && g_simOutage) {
            outage = true;
        }
#endif

        if (outage) {
            m_gridMetrics.voltage = 0.0f;
            m_gridMetrics.frequency = 0.0f;
            m_gridMetrics.isValid = true;
            m_gridMetrics.readErrors = 0;
        } else {
            m_gridMetrics.voltage = 231.5f;
            m_gridMetrics.frequency = 50.0f;
            m_gridMetrics.isValid = true;
            m_gridMetrics.readErrors = 0;
        }

        m_inverterMetrics.voltage = 230.0f;
        m_inverterMetrics.current = 1.45f;
        m_inverterMetrics.power = 320.0f;
        m_inverterMetrics.energy = 1.25f;
        m_inverterMetrics.frequency = 50.0f;
        m_inverterMetrics.powerFactor = 0.96f;
        m_inverterMetrics.isValid = true;
        m_inverterMetrics.readErrors = 0;

        return true;
    }

    if (currentMillis > 0 && m_lastPollTime > 0) {
        if (currentMillis - m_lastPollTime < m_pollIntervalMs) {
            return false;
        }
    }

    m_lastPollTime = currentMillis;
    m_totalPolls++;

    // Inverter output metrology is ALWAYS polled every regular interval
    readInverter();

    // Outage Polling Throttling: When grid is known to be down, throttle grid reads
    // to prevent timeout latency from starving Inverter telemetry or loop execution
    if (m_inOutageMode) {
        if (m_lastGridPollTime == 0 || (currentMillis - m_lastGridPollTime >= m_outagePollIntervalMs)) {
            m_lastGridPollTime = currentMillis;
            readGrid();
        }
    } else {
        m_lastGridPollTime = currentMillis;
        readGrid();
    }

    return true;
}

void PZEMManager::forcePoll() {
    m_totalPolls++;
    bool simActive = m_simulationMode;
#if defined(ARDUINO)
    if (&g_simActive != nullptr && g_simActive) {
        simActive = true;
    }
#endif
    if (simActive) {
        bool outage = m_simOutage;
#if defined(ARDUINO)
        if (&g_simOutage != nullptr && g_simOutage) {
            outage = true;
        }
#endif
        if (outage) {
            m_gridMetrics.voltage = 0.0f;
            m_gridMetrics.frequency = 0.0f;
            m_gridMetrics.isValid = true;
            m_gridMetrics.readErrors = 0;
        } else {
            m_gridMetrics.voltage = 231.5f;
            m_gridMetrics.frequency = 50.0f;
            m_gridMetrics.isValid = true;
            m_gridMetrics.readErrors = 0;
        }
        m_inverterMetrics.voltage = 230.0f;
        m_inverterMetrics.current = 1.45f;
        m_inverterMetrics.power = 320.0f;
        m_inverterMetrics.energy = 1.25f;
        m_inverterMetrics.frequency = 50.0f;
        m_inverterMetrics.powerFactor = 0.96f;
        m_inverterMetrics.isValid = true;
        m_inverterMetrics.readErrors = 0;
        return;
    }
    readGrid();
    readInverter();
}

bool PZEMManager::readGrid() {
    bool success = false;
    PZEMRawReading raw;
    uint8_t retries = 0;

    // Use zero or reduced retries during outage mode to eliminate blocking timeout stalls
    uint8_t allowedRetries = m_inOutageMode ? m_outageMaxRetries : m_maxRetries;

    for (uint8_t attempt = 0; attempt <= allowedRetries; ++attempt) {
#if defined(ARDUINO) && defined(ESP32)
        vTaskDelay(pdMS_TO_TICKS(10)); // Feed FreeRTOS watchdog before every attempt
#endif
        raw = PZEMRawReading{};
        bool transportOk = m_reader->readRaw(m_gridAddr, raw);

        if (transportOk && !std::isnan(raw.voltage) && !std::isnan(raw.frequency) &&
            raw.voltage >= 0.0f && raw.frequency >= 0.0f) {
            success = true;
            retries = attempt;
            break;
        }

        retries = attempt;
#if defined(ARDUINO) && defined(ESP32)
        // 0 retries on hardware if bytes aren't ready
        if (!transportOk) {
            break;
        }
#endif
        if (attempt < allowedRetries) {
            logWarning(m_gridAddr, static_cast<uint8_t>(attempt + 1), allowedRetries);
        }
    }

    m_lastGridRetries = retries;

    if (success) {
        if (m_gridMetrics.readErrors > 0) {
#if defined(ARDUINO)
            Serial.printf("[PZEM 0x%02X] Metrology link restored.\n", m_gridAddr);
#endif
        }
        m_gridMetrics.voltage = raw.voltage;
        m_gridMetrics.frequency = raw.frequency;
        m_gridMetrics.isValid = true;
        m_gridMetrics.readErrors = 0;
    } else {
        if (m_gridMetrics.readErrors == 0) {
            logError(m_gridAddr, allowedRetries);
        }
        m_gridMetrics.isValid = false;
        m_gridMetrics.voltage = 0.0f;
        m_gridMetrics.frequency = 0.0f;
        if (m_gridMetrics.readErrors < 255) {
            m_gridMetrics.readErrors++;
        }
    }

    return success;
}

bool PZEMManager::readInverter() {
    bool success = false;
    PZEMRawReading raw;
    uint8_t retries = 0;

    for (uint8_t attempt = 0; attempt <= m_maxRetries; ++attempt) {
#if defined(ARDUINO) && defined(ESP32)
        vTaskDelay(pdMS_TO_TICKS(10)); // Feed FreeRTOS watchdog before every attempt
#endif
        raw = PZEMRawReading{};
        bool transportOk = m_reader->readRaw(m_inverterAddr, raw);

        if (transportOk &&
            !std::isnan(raw.voltage) && !std::isnan(raw.current) &&
            !std::isnan(raw.power) && !std::isnan(raw.energy) &&
            !std::isnan(raw.frequency) && !std::isnan(raw.powerFactor) &&
            raw.voltage >= 0.0f && raw.current >= 0.0f && raw.power >= 0.0f &&
            raw.energy >= 0.0f && raw.frequency >= 0.0f && raw.powerFactor >= 0.0f) {
            success = true;
            retries = attempt;
            break;
        }

        retries = attempt;
#if defined(ARDUINO) && defined(ESP32)
        // 0 retries on hardware if bytes aren't ready
        if (!transportOk) {
            break;
        }
#endif
        if (attempt < m_maxRetries) {
            logWarning(m_inverterAddr, static_cast<uint8_t>(attempt + 1), m_maxRetries);
        }
    }

    m_lastInverterRetries = retries;

    if (success) {
        if (m_inverterMetrics.readErrors > 0) {
#if defined(ARDUINO)
            Serial.printf("[PZEM 0x%02X] Metrology link restored.\n", m_inverterAddr);
#endif
        }
        m_inverterMetrics.voltage = raw.voltage;
        m_inverterMetrics.current = raw.current;
        m_inverterMetrics.power = raw.power;
        m_inverterMetrics.energy = raw.energy;
        m_inverterMetrics.frequency = raw.frequency;
        m_inverterMetrics.powerFactor = raw.powerFactor;
        m_inverterMetrics.isValid = true;
        m_inverterMetrics.readErrors = 0;
    } else {
        if (m_inverterMetrics.readErrors == 0) {
            logError(m_inverterAddr, m_maxRetries);
        }
        m_inverterMetrics.isValid = false;
        if (m_inverterMetrics.readErrors < 255) {
            m_inverterMetrics.readErrors++;
        }
    }

    return success;
}

void PZEMManager::setCustomReader(IPZEMReader* reader) {
    if (m_ownsReader && m_reader != nullptr) {
        delete m_reader;
    }
    m_reader = reader;
    m_ownsReader = false;
}

void PZEMManager::setMockGridReading(float voltage, float frequency, bool isValid) {
    m_defaultMockReader.setGridReading(voltage, frequency, isValid);
}

void PZEMManager::setMockInverterReading(float voltage, float current, float power, float energy,
                                        float frequency, float powerFactor, bool isValid) {
    m_defaultMockReader.setInverterReading(voltage, current, power, energy, frequency, powerFactor, isValid);
}

void PZEMManager::setMockGridFailures(uint8_t count, bool returnNan) {
    m_defaultMockReader.setGridFailures(count, returnNan);
}

void PZEMManager::setMockInverterFailures(uint8_t count, bool returnNan) {
    m_defaultMockReader.setInverterFailures(count, returnNan);
}

void PZEMManager::reset() {
    m_lastPollTime = 0;
    m_lastGridPollTime = 0;
    m_totalPolls = 0;
    m_lastGridRetries = 0;
    m_lastInverterRetries = 0;
    m_inOutageMode = false;
    m_gridMetrics = GridMetrics{0.0f, 0.0f, false, 0};
    m_inverterMetrics = InverterMetrics{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false, 0};
    m_defaultMockReader.resetCounts();
}

void PZEMManager::logWarning(uint8_t addr, uint8_t attempt, uint8_t maxRetries) {
    if (!m_loggingEnabled) return;
#if defined(ARDUINO)
    Serial.printf("[PZEM 0x%02X] Warning: Read attempt %u/%u failed, retrying...\n",
                  addr, attempt, maxRetries);
#else
    std::printf("[PZEM 0x%02X] Warning: Read attempt %u/%u failed, retrying...\n",
                addr, attempt, maxRetries);
#endif
}

void PZEMManager::logError(uint8_t addr, uint8_t maxRetries) {
    if (!m_loggingEnabled) return;
#if defined(ARDUINO)
    Serial.printf("[PZEM 0x%02X] Error: Read failed after %u retries.\n",
                  addr, maxRetries);
#else
    std::printf("[PZEM 0x%02X] Error: Read failed after %u retries.\n",
                addr, maxRetries);
#endif
}

} // namespace VoltWatch
