#ifndef VOLTWATCH_OUTAGE_STORAGE_H
#define VOLTWATCH_OUTAGE_STORAGE_H

#include <stdint.h>
#include <stddef.h>
#include <string>

#include "include/config.h"
#include "include/types.h"

namespace VoltWatch {

#pragma pack(push, 1)
/**
 * @brief Binary serialization header for NVS outage ring-buffer
 */
struct OutageStorageHeader {
    uint32_t magic;       ///< Magic identifier: 0x56574F55 ("VWOU")
    uint16_t version;     ///< Schema format version (1)
    uint16_t count;       ///< Number of stored outages (0 to 10)
    uint16_t startIndex;  ///< Ring buffer head index (oldest element)
    uint16_t checksum;    ///< Data integrity checksum
};
#pragma pack(pop)

/**
 * @brief Outage history storage manager with flash wear protection.
 *
 * Implements a 10-record ring-buffer for historical power outages.
 * On ESP32, persists records into Non-Volatile Storage (NVS) using Preferences.h.
 * On native systems (macOS), operates in-memory with optional file mocking.
 *
 * Flash Wear Protection:
 * - NO flash writes occur in the 1Hz telemetry polling loop.
 * - Writes occur ONLY on explicit state transitions (outage start, outage end, clear).
 */
class OutageStorage {
public:
    static constexpr size_t MAX_CAPACITY = VoltWatch::Config::OUTAGE_HISTORY_MAX; // 10 records
    static constexpr uint32_t MAGIC_HEADER = 0x56574F55; // "VWOU"
    static constexpr uint16_t CURRENT_VERSION = 1;

    OutageStorage();
    ~OutageStorage() = default;

    /**
     * @brief Initialize storage backend (NVS on ESP32, in-memory/file on native)
     */
    bool begin(const char* mockPath = nullptr);

    // --- State Transition Logging (Flash Writes Only Here) ---
    bool recordOutageStart(uint32_t startTimestamp, float gridVoltageBeforeTrip, float initialLoadWatts);
    bool recordOutageEnd(uint32_t endTimestamp, float energyConsumedKwh);
    bool addRecord(const OutageRecord& record);
    void clear();

    // --- Ring Buffer Queries (Zero Flash Writes) ---
    size_t count() const { return m_count; }
    size_t capacity() const { return MAX_CAPACITY; }
    bool isEmpty() const { return m_count == 0; }
    bool isFull() const { return m_count >= MAX_CAPACITY; }

    /**
     * @brief Retrieve record by reverse chronological order (0 = newest, count - 1 = oldest)
     */
    bool getRecord(size_t index, OutageRecord& record) const;

    /**
     * @brief Retrieve record by chronological order (0 = oldest, count - 1 = newest)
     */
    bool getRecordChronological(size_t index, OutageRecord& record) const;

    /**
     * @brief Pointer to the most recent record, or nullptr if empty
     */
    const OutageRecord* getLatestRecord() const;

    // --- Binary Serialization & Persistence ---
    size_t serialize(uint8_t* buffer, size_t maxLen) const;
    bool deserialize(const uint8_t* buffer, size_t len);
    bool save();
    bool load();

    // --- Diagnostics & Wear Protection Verification ---
    uint32_t getNvsWriteCount() const { return m_nvsWriteCount; }
    void resetNvsWriteCount() { m_nvsWriteCount = 0; }
    void setMockFilePath(const std::string& path) { m_mockFilePath = path; }

private:
    void addRecordInternal(const OutageRecord& record);
    OutageRecord* getLatestRecordMutable();
    static uint16_t computeChecksum(const OutageStorageHeader& header, const OutageRecord* records, size_t recordCount);

    OutageRecord m_records[MAX_CAPACITY];
    size_t m_count{0};
    size_t m_startIndex{0}; // Points to the oldest record in m_records
    uint32_t m_nvsWriteCount{0};
    std::string m_mockFilePath;
};

} // namespace VoltWatch

#endif // VOLTWATCH_OUTAGE_STORAGE_H
