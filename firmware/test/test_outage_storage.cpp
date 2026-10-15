#include "test_framework.h"
#include "../src/storage/OutageStorage.h"
#include <cstring>

TEST_CASE(OutageStorage, InitialState) {
    VoltWatch::OutageStorage storage;

    ASSERT_EQ(size_t(0), storage.count());
    ASSERT_TRUE(storage.isEmpty());
    ASSERT_FALSE(storage.isFull());
    ASSERT_EQ(size_t(10), storage.capacity());
    ASSERT_TRUE(storage.getLatestRecord() == nullptr);

    VoltWatch::OutageRecord rec{};
    ASSERT_FALSE(storage.getRecord(0, rec));
    ASSERT_FALSE(storage.getRecordChronological(0, rec));
}

TEST_CASE(OutageStorage, AddUpToCapacity) {
    VoltWatch::OutageStorage storage;

    for (uint32_t i = 1; i <= 10; ++i) {
        VoltWatch::OutageRecord rec{};
        rec.startTimestamp = 1000 * i;
        rec.endTimestamp = 1000 * i + 300;
        rec.durationSeconds = 300;
        rec.gridVoltageBeforeTrip = 230.0f + static_cast<float>(i);
        rec.initialLoadWatts = 300.0f + static_cast<float>(i * 10);
        rec.energyConsumedKwh = 0.05f * static_cast<float>(i);

        bool ok = storage.addRecord(rec);
        ASSERT_TRUE(ok);
        ASSERT_EQ(size_t(i), storage.count());
    }

    ASSERT_TRUE(storage.isFull());
    ASSERT_FALSE(storage.isEmpty());
    ASSERT_EQ(size_t(10), storage.count());

    // Verify reverse chronological retrieval (0 is newest = 10000s)
    VoltWatch::OutageRecord newest{};
    ASSERT_TRUE(storage.getRecord(0, newest));
    ASSERT_EQ(uint32_t(10000), newest.startTimestamp);

    VoltWatch::OutageRecord oldest{};
    ASSERT_TRUE(storage.getRecord(9, oldest));
    ASSERT_EQ(uint32_t(1000), oldest.startTimestamp);

    // Verify chronological retrieval (0 is oldest = 1000s, 9 is newest = 10000s)
    VoltWatch::OutageRecord chrono0{};
    ASSERT_TRUE(storage.getRecordChronological(0, chrono0));
    ASSERT_EQ(uint32_t(1000), chrono0.startTimestamp);

    VoltWatch::OutageRecord chrono9{};
    ASSERT_TRUE(storage.getRecordChronological(9, chrono9));
    ASSERT_EQ(uint32_t(10000), chrono9.startTimestamp);
}

TEST_CASE(OutageStorage, FifoOverwriteOn11thRecord) {
    VoltWatch::OutageStorage storage;

    // Fill with records 1 to 10
    for (uint32_t i = 1; i <= 10; ++i) {
        VoltWatch::OutageRecord rec{};
        rec.startTimestamp = 1000 * i;
        rec.endTimestamp = 1000 * i + 100;
        rec.durationSeconds = 100;
        rec.gridVoltageBeforeTrip = 230.0f;
        rec.initialLoadWatts = 300.0f;
        rec.energyConsumedKwh = 0.05f;
        storage.addRecord(rec);
    }
    ASSERT_EQ(size_t(10), storage.count());

    // Add 11th record (timestamp = 11000)
    VoltWatch::OutageRecord rec11{};
    rec11.startTimestamp = 11000;
    rec11.endTimestamp = 11200;
    rec11.durationSeconds = 200;
    rec11.gridVoltageBeforeTrip = 228.0f;
    rec11.initialLoadWatts = 450.0f;
    rec11.energyConsumedKwh = 0.08f;

    storage.addRecord(rec11);

    // Count must remain capped at 10
    ASSERT_EQ(size_t(10), storage.count());
    ASSERT_TRUE(storage.isFull());

    // Oldest record (1000) should be evicted. Chrono 0 should now be 2000!
    VoltWatch::OutageRecord oldestAfterEvict{};
    ASSERT_TRUE(storage.getRecordChronological(0, oldestAfterEvict));
    ASSERT_EQ(uint32_t(2000), oldestAfterEvict.startTimestamp);

    // Newest record should be 11000
    VoltWatch::OutageRecord newestAfterEvict{};
    ASSERT_TRUE(storage.getRecord(0, newestAfterEvict));
    ASSERT_EQ(uint32_t(11000), newestAfterEvict.startTimestamp);

    // Add 12th record (timestamp = 12000)
    VoltWatch::OutageRecord rec12{};
    rec12.startTimestamp = 12000;
    storage.addRecord(rec12);

    ASSERT_EQ(size_t(10), storage.count());
    ASSERT_TRUE(storage.getRecordChronological(0, oldestAfterEvict));
    ASSERT_EQ(uint32_t(3000), oldestAfterEvict.startTimestamp);
}

TEST_CASE(OutageStorage, StateTransitionLogging) {
    VoltWatch::OutageStorage storage;

    bool startOk = storage.recordOutageStart(50000, 224.5f, 520.0f);
    ASSERT_TRUE(startOk);
    ASSERT_EQ(size_t(1), storage.count());

    const VoltWatch::OutageRecord* latest = storage.getLatestRecord();
    ASSERT_TRUE(latest != nullptr);
    ASSERT_EQ(uint32_t(50000), latest->startTimestamp);
    ASSERT_EQ(uint32_t(0), latest->endTimestamp);
    ASSERT_EQ(uint32_t(0), latest->durationSeconds);
    ASSERT_FLOAT_NEAR(224.5f, latest->gridVoltageBeforeTrip, 0.01f);
    ASSERT_FLOAT_NEAR(520.0f, latest->initialLoadWatts, 0.01f);

    // End outage 1800 seconds later with 0.26 kWh consumed
    bool endOk = storage.recordOutageEnd(51800, 0.260f);
    ASSERT_TRUE(endOk);
    ASSERT_EQ(size_t(1), storage.count());

    latest = storage.getLatestRecord();
    ASSERT_TRUE(latest != nullptr);
    ASSERT_EQ(uint32_t(51800), latest->endTimestamp);
    ASSERT_EQ(uint32_t(1800), latest->durationSeconds);
    ASSERT_FLOAT_NEAR(0.260f, latest->energyConsumedKwh, 0.001f);
}

TEST_CASE(OutageStorage, FlashWearProtection) {
    VoltWatch::OutageStorage storage;
    storage.resetNvsWriteCount();
    ASSERT_EQ(uint32_t(0), storage.getNvsWriteCount());

    // Simulate 1000 cycles of 1Hz telemetry polling (queries only)
    for (int i = 0; i < 1000; ++i) {
        (void)storage.count();
        (void)storage.isFull();
        (void)storage.isEmpty();
        VoltWatch::OutageRecord rec{};
        (void)storage.getRecord(0, rec);
        (void)storage.getLatestRecord();
    }

    // Flash write count MUST be zero during read-only polling
    ASSERT_EQ(uint32_t(0), storage.getNvsWriteCount());

    // Flash write happens strictly on state transitions
    storage.recordOutageStart(100000, 220.0f, 350.0f);
    ASSERT_EQ(uint32_t(1), storage.getNvsWriteCount());

    // Another 500 read cycles
    for (int i = 0; i < 500; ++i) {
        VoltWatch::OutageRecord rec{};
        (void)storage.getRecord(0, rec);
    }
    ASSERT_EQ(uint32_t(1), storage.getNvsWriteCount());

    storage.recordOutageEnd(103600, 0.35f);
    ASSERT_EQ(uint32_t(2), storage.getNvsWriteCount());
}

TEST_CASE(OutageStorage, SerializationDeserialization) {
    VoltWatch::OutageStorage original;

    // Fill with 10 records
    for (uint32_t i = 1; i <= 10; ++i) {
        VoltWatch::OutageRecord rec{};
        rec.startTimestamp = 20000 + i * 1000;
        rec.endTimestamp = 20000 + i * 1000 + 500;
        rec.durationSeconds = 500;
        rec.gridVoltageBeforeTrip = 230.0f + static_cast<float>(i);
        rec.initialLoadWatts = 400.0f + static_cast<float>(i * 15);
        rec.energyConsumedKwh = 0.055f * static_cast<float>(i);
        original.addRecord(rec);
    }

    uint8_t buffer[512];
    size_t serializedLen = original.serialize(buffer, sizeof(buffer));
    ASSERT_TRUE(serializedLen > 0);

    // Verify magic header "VWOU" = 0x56574F55 (in little endian: 0x55, 0x4F, 0x57, 0x56)
    uint32_t magic = 0;
    std::memcpy(&magic, buffer, sizeof(uint32_t));
    ASSERT_EQ(VoltWatch::OutageStorage::MAGIC_HEADER, magic);

    // Deserialize into fresh instance
    VoltWatch::OutageStorage restored;
    ASSERT_EQ(size_t(0), restored.count());

    bool desOk = restored.deserialize(buffer, serializedLen);
    ASSERT_TRUE(desOk);
    ASSERT_EQ(size_t(10), restored.count());

    // Verify all 10 records match exactly
    for (size_t i = 0; i < 10; ++i) {
        VoltWatch::OutageRecord r1{}, r2{};
        ASSERT_TRUE(original.getRecord(i, r1));
        ASSERT_TRUE(restored.getRecord(i, r2));

        ASSERT_EQ(r1.startTimestamp, r2.startTimestamp);
        ASSERT_EQ(r1.endTimestamp, r2.endTimestamp);
        ASSERT_EQ(r1.durationSeconds, r2.durationSeconds);
        ASSERT_FLOAT_NEAR(r1.gridVoltageBeforeTrip, r2.gridVoltageBeforeTrip, 0.01f);
        ASSERT_FLOAT_NEAR(r1.initialLoadWatts, r2.initialLoadWatts, 0.01f);
        ASSERT_FLOAT_NEAR(r1.energyConsumedKwh, r2.energyConsumedKwh, 0.0001f);
    }

    // Corrupt magic identifier: deserialize must fail
    uint8_t corrupted[512];
    std::memcpy(corrupted, buffer, serializedLen);
    corrupted[0] = 0xFF;
    ASSERT_FALSE(restored.deserialize(corrupted, serializedLen));

    // Corrupt record data (checksum mismatch): deserialize must fail
    std::memcpy(corrupted, buffer, serializedLen);
    corrupted[sizeof(VoltWatch::OutageStorageHeader) + 5] ^= 0xAA;
    ASSERT_FALSE(restored.deserialize(corrupted, serializedLen));
}
