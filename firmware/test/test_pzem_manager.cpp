#include "test_framework.h"
#include "../src/metrology/PZEMManager.h"

TEST_CASE(PZEMManager, InitialStateAndDefaults) {
    VoltWatch::PZEMManager manager;

    ASSERT_FALSE(manager.isGridValid());
    ASSERT_FALSE(manager.isInverterValid());
    ASSERT_EQ(0, manager.getGridReadErrors());
    ASSERT_EQ(0, manager.getInverterReadErrors());
    ASSERT_EQ(1000u, manager.getPollInterval());
    ASSERT_EQ(3, manager.getMaxRetries());
    ASSERT_EQ(0u, manager.getTotalPolls());
}

TEST_CASE(PZEMManager, SequentialPollingTiming) {
    VoltWatch::PZEMManager manager;

    // First poll at t=1000ms: executes
    bool polled1 = manager.poll(1000);
    ASSERT_TRUE(polled1);
    ASSERT_EQ(1u, manager.getTotalPolls());
    ASSERT_EQ(1000u, manager.getLastPollTime());

    // Second poll at t=1500ms (only 500ms elapsed < 1000ms interval): skipped
    bool polled2 = manager.poll(1500);
    ASSERT_FALSE(polled2);
    ASSERT_EQ(1u, manager.getTotalPolls());

    // Third poll at t=2000ms (1000ms elapsed): executes
    bool polled3 = manager.poll(2000);
    ASSERT_TRUE(polled3);
    ASSERT_EQ(2u, manager.getTotalPolls());
    ASSERT_EQ(2000u, manager.getLastPollTime());
}

TEST_CASE(PZEMManager, SequentialReadExecution) {
    VoltWatch::PZEMManager manager;
    VoltWatch::MockPZEMReader* mock = manager.getMockReader();
    mock->resetCounts();

    manager.forcePoll();

    // Verify both modules were read sequentially
    ASSERT_EQ(1u, mock->getGridReadCount());
    ASSERT_EQ(1u, mock->getInverterReadCount());
}

TEST_CASE(PZEMManager, SuccessfulReadingDataIntegrity) {
    VoltWatch::PZEMManager manager;
    manager.setMockGridReading(234.5f, 50.1f, true);
    manager.setMockInverterReading(229.8f, 2.50f, 574.5f, 15.20f, 49.9f, 0.98f, true);

    manager.forcePoll();

    ASSERT_TRUE(manager.isGridValid());
    ASSERT_FLOAT_NEAR(234.5f, manager.getGridMetrics().voltage, 0.01f);
    ASSERT_FLOAT_NEAR(50.1f, manager.getGridMetrics().frequency, 0.01f);
    ASSERT_EQ(0, manager.getGridReadErrors());

    ASSERT_TRUE(manager.isInverterValid());
    ASSERT_FLOAT_NEAR(229.8f, manager.getInverterMetrics().voltage, 0.01f);
    ASSERT_FLOAT_NEAR(2.50f, manager.getInverterMetrics().current, 0.01f);
    ASSERT_FLOAT_NEAR(574.5f, manager.getInverterMetrics().power, 0.01f);
    ASSERT_FLOAT_NEAR(15.20f, manager.getInverterMetrics().energy, 0.01f);
    ASSERT_FLOAT_NEAR(49.9f, manager.getInverterMetrics().frequency, 0.01f);
    ASSERT_FLOAT_NEAR(0.98f, manager.getInverterMetrics().powerFactor, 0.01f);
    ASSERT_EQ(0, manager.getInverterReadErrors());
}

TEST_CASE(PZEMManager, ModbusTimeoutWith3RetriesSuccess) {
    VoltWatch::PZEMManager manager;

    // Test Case A: 1 failure then success (attempt 0 fails, retry 1 succeeds)
    manager.setMockGridFailures(1, false);
    bool ok1 = manager.readGrid();
    ASSERT_TRUE(ok1);
    ASSERT_TRUE(manager.isGridValid());
    ASSERT_EQ(1, manager.getLastGridRetries());
    ASSERT_EQ(0, manager.getGridReadErrors());

    // Test Case B: 2 failures then success (retry 2 succeeds)
    manager.setMockGridFailures(2, false);
    bool ok2 = manager.readGrid();
    ASSERT_TRUE(ok2);
    ASSERT_TRUE(manager.isGridValid());
    ASSERT_EQ(2, manager.getLastGridRetries());
    ASSERT_EQ(0, manager.getGridReadErrors());

    // Test Case C: 3 failures then success (retry 3 succeeds)
    manager.setMockGridFailures(3, false);
    bool ok3 = manager.readGrid();
    ASSERT_TRUE(ok3);
    ASSERT_TRUE(manager.isGridValid());
    ASSERT_EQ(3, manager.getLastGridRetries());
    ASSERT_EQ(0, manager.getGridReadErrors());
}

TEST_CASE(PZEMManager, ModbusTimeoutRetriesExhaustedFails) {
    VoltWatch::PZEMManager manager;

    // 4 failures (initial attempt + 3 retries all fail)
    manager.setMockGridFailures(4, false);
    bool ok = manager.readGrid();

    ASSERT_FALSE(ok);
    ASSERT_FALSE(manager.isGridValid());
    ASSERT_EQ(1, manager.getGridReadErrors());
    ASSERT_EQ(3, manager.getLastGridRetries());
}

TEST_CASE(PZEMManager, NaNReadingHandlingWithRetries) {
    VoltWatch::PZEMManager manager;

    // Case A: 2 consecutive NaN readings, then valid readings
    manager.setMockGridFailures(2, true); // true = return NaN
    bool okRecovered = manager.readGrid();

    ASSERT_TRUE(okRecovered);
    ASSERT_TRUE(manager.isGridValid());
    ASSERT_EQ(2, manager.getLastGridRetries());
    ASSERT_EQ(0, manager.getGridReadErrors());

    // Case B: 4 consecutive NaN readings (all retries exhausted)
    manager.setMockGridFailures(4, true);
    bool okFailed = manager.readGrid();

    ASSERT_FALSE(okFailed);
    ASSERT_FALSE(manager.isGridValid());
    ASSERT_EQ(1, manager.getGridReadErrors());
}

TEST_CASE(PZEMManager, RecoveryAfterConsecutiveFailures) {
    VoltWatch::PZEMManager manager;

    // Fail first cycle
    manager.setMockGridFailures(4, false);
    manager.readGrid();
    ASSERT_FALSE(manager.isGridValid());
    ASSERT_EQ(1, manager.getGridReadErrors());

    // Fail second cycle
    manager.setMockGridFailures(4, false);
    manager.readGrid();
    ASSERT_FALSE(manager.isGridValid());
    ASSERT_EQ(2, manager.getGridReadErrors());

    // Third cycle succeeds immediately
    manager.readGrid();
    ASSERT_TRUE(manager.isGridValid());
    ASSERT_EQ(0, manager.getGridReadErrors());
}

TEST_CASE(PZEMManager, InverterReadFailureHandling) {
    VoltWatch::PZEMManager manager;

    // Inverter module fails 4 times (all retries exhausted)
    manager.setMockInverterFailures(4, false);
    manager.forcePoll();

    // Grid remains healthy
    ASSERT_TRUE(manager.isGridValid());
    ASSERT_EQ(0, manager.getGridReadErrors());

    // Inverter read fails
    ASSERT_FALSE(manager.isInverterValid());
    ASSERT_EQ(1, manager.getInverterReadErrors());
}

TEST_CASE(PZEMManager, OutageThrottlingBehavior) {
    VoltWatch::PZEMManager manager;
    manager.setOutagePollInterval(3000); // 3-second throttle during outage
    manager.setOutageMaxRetries(0);      // 0 retries during outage
    manager.setOutageMode(true);
    ASSERT_TRUE(manager.isOutageMode());

    // t=1000ms: First poll in outage mode reads both Inverter and Grid (initial probe)
    manager.setMockGridFailures(1, false); // fails 1 attempt
    bool polled1 = manager.poll(1000);
    ASSERT_TRUE(polled1);
    ASSERT_FALSE(manager.isGridValid());
    ASSERT_EQ(0, manager.getLastGridRetries()); // 0 retries used (no blocking stall)

    // t=2000ms: 1 second later: Inverter polled, but Grid poll is throttled/skipped!
    uint32_t gridReadsBefore = manager.getMockReader()->getGridReadCount();
    uint32_t invReadsBefore = manager.getMockReader()->getInverterReadCount();
    bool polled2 = manager.poll(2000);
    ASSERT_TRUE(polled2);
    ASSERT_EQ(invReadsBefore + 1, manager.getMockReader()->getInverterReadCount());
    ASSERT_EQ(gridReadsBefore, manager.getMockReader()->getGridReadCount()); // Grid skipped!

    // t=3000ms: 2 seconds later: Grid still skipped
    manager.poll(3000);
    ASSERT_EQ(gridReadsBefore, manager.getMockReader()->getGridReadCount());

    // t=4000ms: 3 seconds elapsed: Grid probe fires!
    manager.poll(4000);
    ASSERT_EQ(gridReadsBefore + 1, manager.getMockReader()->getGridReadCount());
}
