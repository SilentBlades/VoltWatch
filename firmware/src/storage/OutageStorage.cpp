#include "OutageStorage.h"

#include <cstring>
#include <cstdio>

#if defined(ARDUINO)
#include <Arduino.h>
#include <Preferences.h>
#endif

namespace VoltWatch {

OutageStorage::OutageStorage() {
    std::memset(m_records, 0, sizeof(m_records));
}

bool OutageStorage::begin(const char* mockPath) {
    if (mockPath) {
        m_mockFilePath = mockPath;
    }
    return load();
}

uint16_t OutageStorage::computeChecksum(const OutageStorageHeader& header, const OutageRecord* records, size_t recordCount) {
    uint16_t sum = static_cast<uint16_t>(header.magic ^ (header.magic >> 16))
                 + header.version
                 + header.count
                 + header.startIndex;

    const uint8_t* bytePtr = reinterpret_cast<const uint8_t*>(records);
    size_t byteCount = recordCount * sizeof(OutageRecord);
    for (size_t i = 0; i < byteCount; ++i) {
        sum += bytePtr[i];
    }
    return sum;
}

void OutageStorage::addRecordInternal(const OutageRecord& record) {
    if (m_count < MAX_CAPACITY) {
        size_t insertIdx = (m_startIndex + m_count) % MAX_CAPACITY;
        m_records[insertIdx] = record;
        m_count++;
    } else {
        // Buffer is at full capacity (10): overwrite oldest record (FIFO)
        m_records[m_startIndex] = record;
        m_startIndex = (m_startIndex + 1) % MAX_CAPACITY;
    }
}

bool OutageStorage::addRecord(const OutageRecord& record) {
    addRecordInternal(record);
    return save();
}

bool OutageStorage::recordOutageStart(uint32_t startTimestamp, float gridVoltageBeforeTrip, float initialLoadWatts) {
    OutageRecord rec{};
    rec.startTimestamp = startTimestamp;
    rec.endTimestamp = 0;
    rec.durationSeconds = 0;
    rec.gridVoltageBeforeTrip = gridVoltageBeforeTrip;
    rec.initialLoadWatts = initialLoadWatts;
    rec.energyConsumedKwh = 0.0f;

    addRecordInternal(rec);
    return save();
}

bool OutageStorage::recordOutageEnd(uint32_t endTimestamp, float energyConsumedKwh) {
    OutageRecord* latest = getLatestRecordMutable();
    if (!latest) {
        return false;
    }

    latest->endTimestamp = endTimestamp;
    if (endTimestamp >= latest->startTimestamp) {
        latest->durationSeconds = endTimestamp - latest->startTimestamp;
    } else {
        latest->durationSeconds = 0;
    }
    latest->energyConsumedKwh = energyConsumedKwh;

    return save();
}

bool OutageStorage::getRecord(size_t index, OutageRecord& record) const {
    if (index >= m_count) {
        return false;
    }
    // Reverse chronological order: 0 is newest
    size_t physicalIdx = (m_startIndex + m_count - 1 - index) % MAX_CAPACITY;
    record = m_records[physicalIdx];
    return true;
}

bool OutageStorage::getRecordChronological(size_t index, OutageRecord& record) const {
    if (index >= m_count) {
        return false;
    }
    // Chronological order: 0 is oldest
    size_t physicalIdx = (m_startIndex + index) % MAX_CAPACITY;
    record = m_records[physicalIdx];
    return true;
}

const OutageRecord* OutageStorage::getLatestRecord() const {
    if (m_count == 0) {
        return nullptr;
    }
    size_t newestIdx = (m_startIndex + m_count - 1) % MAX_CAPACITY;
    return &m_records[newestIdx];
}

OutageRecord* OutageStorage::getLatestRecordMutable() {
    if (m_count == 0) {
        return nullptr;
    }
    size_t newestIdx = (m_startIndex + m_count - 1) % MAX_CAPACITY;
    return &m_records[newestIdx];
}

void OutageStorage::clear() {
    m_count = 0;
    m_startIndex = 0;
    std::memset(m_records, 0, sizeof(m_records));
    save();
}

size_t OutageStorage::serialize(uint8_t* buffer, size_t maxLen) const {
    size_t required = sizeof(OutageStorageHeader) + (MAX_CAPACITY * sizeof(OutageRecord));
    if (!buffer || maxLen < required) {
        return 0;
    }

    OutageStorageHeader header{};
    header.magic = MAGIC_HEADER;
    header.version = CURRENT_VERSION;
    header.count = static_cast<uint16_t>(m_count);
    header.startIndex = static_cast<uint16_t>(m_startIndex);
    header.checksum = computeChecksum(header, m_records, MAX_CAPACITY);

    std::memcpy(buffer, &header, sizeof(OutageStorageHeader));
    std::memcpy(buffer + sizeof(OutageStorageHeader), m_records, MAX_CAPACITY * sizeof(OutageRecord));

    return required;
}

bool OutageStorage::deserialize(const uint8_t* buffer, size_t len) {
    size_t required = sizeof(OutageStorageHeader) + (MAX_CAPACITY * sizeof(OutageRecord));
    if (!buffer || len < required) {
        return false;
    }

    OutageStorageHeader header{};
    std::memcpy(&header, buffer, sizeof(OutageStorageHeader));

    if (header.magic != MAGIC_HEADER || header.version != CURRENT_VERSION) {
        return false;
    }

    if (header.count > MAX_CAPACITY) {
        return false;
    }

    if (header.count > 0 && header.startIndex >= MAX_CAPACITY) {
        return false;
    }

    const OutageRecord* recordsPtr = reinterpret_cast<const OutageRecord*>(buffer + sizeof(OutageStorageHeader));
    uint16_t expectedChecksum = computeChecksum(header, recordsPtr, MAX_CAPACITY);
    if (header.checksum != expectedChecksum) {
        return false;
    }

    m_count = header.count;
    m_startIndex = header.startIndex;
    std::memcpy(m_records, recordsPtr, MAX_CAPACITY * sizeof(OutageRecord));

    return true;
}

bool OutageStorage::save() {
    m_nvsWriteCount++;

#if defined(ARDUINO)
    Preferences prefs;
    if (!prefs.begin("voltwatch_out", false)) {
        return false;
    }
    uint8_t buffer[sizeof(OutageStorageHeader) + (MAX_CAPACITY * sizeof(OutageRecord))];
    size_t written = serialize(buffer, sizeof(buffer));
    if (written == 0) {
        prefs.end();
        return false;
    }
    size_t stored = prefs.putBytes("history", buffer, written);
    prefs.end();
    return stored == written;
#else
    if (!m_mockFilePath.empty()) {
        uint8_t buffer[sizeof(OutageStorageHeader) + (MAX_CAPACITY * sizeof(OutageRecord))];
        size_t written = serialize(buffer, sizeof(buffer));
        if (written > 0) {
            FILE* fp = std::fopen(m_mockFilePath.c_str(), "wb");
            if (fp) {
                std::fwrite(buffer, 1, written, fp);
                std::fclose(fp);
            }
        }
    }
    return true;
#endif
}

bool OutageStorage::load() {
#if defined(ARDUINO)
    Preferences prefs;
    if (!prefs.begin("voltwatch_out", true)) {
        return false;
    }
    uint8_t buffer[sizeof(OutageStorageHeader) + (MAX_CAPACITY * sizeof(OutageRecord))];
    size_t readLen = prefs.getBytes("history", buffer, sizeof(buffer));
    prefs.end();
    if (readLen > 0) {
        return deserialize(buffer, readLen);
    }
    return false;
#else
    if (!m_mockFilePath.empty()) {
        FILE* fp = std::fopen(m_mockFilePath.c_str(), "rb");
        if (fp) {
            uint8_t buffer[sizeof(OutageStorageHeader) + (MAX_CAPACITY * sizeof(OutageRecord))];
            size_t readLen = std::fread(buffer, 1, sizeof(buffer), fp);
            std::fclose(fp);
            if (readLen > 0) {
                return deserialize(buffer, readLen);
            }
        }
    }
    return true;
#endif
}

} // namespace VoltWatch
