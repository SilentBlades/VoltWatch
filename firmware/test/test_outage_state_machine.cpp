#include "test_framework.h"
#include "../src/statemachine/OutageStateMachine.h"

TEST_CASE(OutageStateMachine, InitialNormalState) {
    VoltWatch::OutageStateMachine sm;
    sm.init(230.0f, true, 5.0f, 250.0f, 100);

    ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_NORMAL), static_cast<int>(sm.getState()));
    ASSERT_TRUE(sm.isNormal());
    ASSERT_FALSE(sm.isOutage());
    ASSERT_FALSE(sm.isRecovered());
    ASSERT_FALSE(sm.wasBootOutage());
    ASSERT_FALSE(sm.hasStateChanged());
}

TEST_CASE(OutageStateMachine, DebounceOutageTransition) {
    VoltWatch::OutageStateMachine sm(170.0f, 80.0f, 3);
    sm.init(230.0f, true, 10.0f, 300.0f, 100);

    // Grid drops to 0V (Outage detected)
    // Cycle 1: candidate count = 1, state still NORMAL
    sm.update(0.0f, true, 10.0f, 300.0f, 101);
    ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_NORMAL), static_cast<int>(sm.getState()));
    ASSERT_FALSE(sm.hasStateChanged());
    ASSERT_EQ(1, sm.getCandidateCount());

    // Cycle 2: candidate count = 2, state still NORMAL
    sm.update(0.0f, true, 10.0f, 300.0f, 102);
    ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_NORMAL), static_cast<int>(sm.getState()));
    ASSERT_FALSE(sm.hasStateChanged());
    ASSERT_EQ(2, sm.getCandidateCount());

    // Cycle 3: candidate count reaches 3, state commits to STATE_OUTAGE
    sm.update(0.0f, true, 10.0f, 300.0f, 103);
    ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_OUTAGE), static_cast<int>(sm.getState()));
    ASSERT_TRUE(sm.hasStateChanged());
    ASSERT_TRUE(sm.isOutage());
    ASSERT_EQ(0, sm.getCandidateCount());

    // Cycle 4: remains in STATE_OUTAGE, hasStateChanged resets to false
    sm.update(0.0f, true, 10.0f, 300.0f, 104);
    ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_OUTAGE), static_cast<int>(sm.getState()));
    ASSERT_FALSE(sm.hasStateChanged());
}

TEST_CASE(OutageStateMachine, TransientGlitchNoiseRejection) {
    VoltWatch::OutageStateMachine sm(170.0f, 80.0f, 3);
    sm.init(230.0f, true, 10.0f, 300.0f, 100);

    // Transient dip: 2 cycles of 0V, then restored to 230V
    sm.update(0.0f, true, 10.0f, 300.0f, 101);
    ASSERT_EQ(1, sm.getCandidateCount());

    sm.update(0.0f, true, 10.0f, 300.0f, 102);
    ASSERT_EQ(2, sm.getCandidateCount());
    ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_NORMAL), static_cast<int>(sm.getState()));

    // Grid bounces back to healthy 230V on cycle 3
    sm.update(230.0f, true, 10.0f, 300.0f, 103);
    ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_NORMAL), static_cast<int>(sm.getState()));
    ASSERT_FALSE(sm.hasStateChanged());
    ASSERT_EQ(0, sm.getCandidateCount());
}

TEST_CASE(OutageStateMachine, DebounceRecoveryTransition) {
    VoltWatch::OutageStateMachine sm(170.0f, 80.0f, 3);
    sm.init(230.0f, true, 10.0f, 300.0f, 100);

    // Transition to outage (3 cycles)
    sm.update(0.0f, true, 10.0f, 300.0f, 101);
    sm.update(0.0f, true, 10.0f, 300.0f, 102);
    sm.update(0.0f, true, 10.0f, 300.0f, 103);
    ASSERT_TRUE(sm.isOutage());

    // Grid returns to 230V
    // Recovery Cycle 1
    sm.update(230.0f, true, 10.5f, 300.0f, 104);
    ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_OUTAGE), static_cast<int>(sm.getState()));
    ASSERT_FALSE(sm.hasStateChanged());
    ASSERT_EQ(1, sm.getCandidateCount());

    // Recovery Cycle 2
    sm.update(230.0f, true, 10.5f, 300.0f, 105);
    ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_OUTAGE), static_cast<int>(sm.getState()));
    ASSERT_FALSE(sm.hasStateChanged());
    ASSERT_EQ(2, sm.getCandidateCount());

    // Recovery Cycle 3 -> commits to STATE_RECOVERED
    sm.update(230.0f, true, 10.5f, 300.0f, 106);
    ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_RECOVERED), static_cast<int>(sm.getState()));
    ASSERT_TRUE(sm.hasStateChanged());
    ASSERT_TRUE(sm.isRecovered());
    ASSERT_EQ(0, sm.getCandidateCount());
}

TEST_CASE(OutageStateMachine, BootDuringOutage) {
    // When ESP32 powers up from inverter rail during grid outage (0V or Modbus error)
    VoltWatch::OutageStateMachine sm(170.0f, 80.0f, 3);
    sm.init(0.0f, true, 20.0f, 400.0f, 50);

    // Must be STATE_OUTAGE immediately
    ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_OUTAGE), static_cast<int>(sm.getState()));
    ASSERT_TRUE(sm.wasBootOutage());
    // CRITICAL: Must NOT fire an alert on boot!
    ASSERT_FALSE(sm.hasStateChanged());

    // Ongoing outage updates do not change state or fire alert
    sm.update(0.0f, true, 20.1f, 400.0f, 51);
    ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_OUTAGE), static_cast<int>(sm.getState()));
    ASSERT_FALSE(sm.hasStateChanged());

    // When power returns, debounces to STATE_RECOVERED
    sm.update(230.0f, true, 20.2f, 300.0f, 52);
    sm.update(230.0f, true, 20.2f, 300.0f, 53);
    sm.update(230.0f, true, 20.2f, 300.0f, 54);
    ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_RECOVERED), static_cast<int>(sm.getState()));
    ASSERT_TRUE(sm.hasStateChanged());
}

TEST_CASE(OutageStateMachine, ModbusTimeoutTriggersOutage) {
    VoltWatch::OutageStateMachine sm(170.0f, 80.0f, 3);
    sm.init(230.0f, true, 5.0f, 200.0f, 100);

    // Modbus timeout / read error indicated by gridValid = false
    sm.update(0.0f, false, 5.0f, 200.0f, 101);
    sm.update(0.0f, false, 5.0f, 200.0f, 102);
    sm.update(0.0f, false, 5.0f, 200.0f, 103);

    ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_OUTAGE), static_cast<int>(sm.getState()));
    ASSERT_TRUE(sm.hasStateChanged());
}

TEST_CASE(OutageStateMachine, HysteresisIndeterminateBand) {
    VoltWatch::OutageStateMachine sm(170.0f, 80.0f, 3);
    sm.init(230.0f, true, 5.0f, 200.0f, 100);

    // Grid sags to 120V (between 80V outage and 170V present thresholds)
    // Should maintain STATE_NORMAL
    for (int i = 0; i < 5; ++i) {
        sm.update(120.0f, true, 5.0f, 200.0f, 101 + i);
        ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_NORMAL), static_cast<int>(sm.getState()));
    }

    // Now voltage drops to 70V (< 80V) -> 3 cycles triggers outage
    sm.update(70.0f, true, 5.0f, 200.0f, 106);
    sm.update(70.0f, true, 5.0f, 200.0f, 107);
    sm.update(70.0f, true, 5.0f, 200.0f, 108);
    ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_OUTAGE), static_cast<int>(sm.getState()));

    // Voltage rises to 140V (still below 170V recovery threshold)
    // Should maintain STATE_OUTAGE
    for (int i = 0; i < 5; ++i) {
        sm.update(140.0f, true, 5.0f, 200.0f, 109 + i);
        ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_OUTAGE), static_cast<int>(sm.getState()));
    }

    // Voltage reaches 175V (>= 170V) -> 3 cycles triggers recovery
    sm.update(175.0f, true, 5.0f, 200.0f, 114);
    sm.update(175.0f, true, 5.0f, 200.0f, 115);
    sm.update(175.0f, true, 5.0f, 200.0f, 116);
    ASSERT_EQ(static_cast<int>(VoltWatch::OutageState::STATE_RECOVERED), static_cast<int>(sm.getState()));
}

TEST_CASE(OutageStateMachine, DurationAndEnergyTracking) {
    VoltWatch::OutageStateMachine sm(170.0f, 80.0f, 3);
    sm.init(230.0f, true, 10.0f, 350.0f, 1000);

    // Outage starts at t=1003, trip energy = 10.0 kWh
    sm.update(0.0f, true, 10.0f, 350.0f, 1001);
    sm.update(0.0f, true, 10.0f, 350.0f, 1002);
    sm.update(0.0f, true, 10.0f, 350.0f, 1003);
    ASSERT_TRUE(sm.isOutage());
    ASSERT_EQ(1003u, sm.getOutageStartTimestamp());

    // Active outage at t=1103 (100 seconds later), energy = 10.5 kWh
    ASSERT_EQ(100u, sm.getActiveOutageDuration(1103));
    ASSERT_FLOAT_NEAR(0.5f, sm.getActiveOutageEnergy(10.5f), 0.001f);

    // Recovery starts at t=1600, restores at t=1603 with inverter energy = 12.5 kWh
    sm.update(230.0f, true, 12.5f, 300.0f, 1601);
    sm.update(230.0f, true, 12.5f, 300.0f, 1602);
    sm.update(230.0f, true, 12.5f, 300.0f, 1603);
    ASSERT_TRUE(sm.isRecovered());

    // Active duration/energy reset to 0
    ASSERT_EQ(0u, sm.getActiveOutageDuration(1603));
    ASSERT_FLOAT_NEAR(0.0f, sm.getActiveOutageEnergy(12.5f), 0.001f);

    // Completed record verification: duration = 1603 - 1003 = 600s, energy delta = 12.5 - 10.0 = 2.5 kWh
    const VoltWatch::OutageRecord& rec = sm.getLastOutageRecord();
    ASSERT_EQ(1003u, rec.startTimestamp);
    ASSERT_EQ(1603u, rec.endTimestamp);
    ASSERT_EQ(600u, rec.durationSeconds);
    ASSERT_FLOAT_NEAR(2.5f, rec.energyConsumedKwh, 0.001f);
    ASSERT_FLOAT_NEAR(230.0f, rec.gridVoltageBeforeTrip, 0.1f);
    ASSERT_FLOAT_NEAR(350.0f, rec.initialLoadWatts, 0.1f);
}

TEST_CASE(OutageStateMachine, BatteryRuntimeEstimation) {
    // 12V 150Ah @ 85% eff and 80% DoD:
    // Total DC = 1800 Wh
    // Usable DC = 1440 Wh
    // Usable AC = 1224 Wh
    const VoltWatch::BatteryConfig config = {12.0f, 150.0f, 0.85f, 0.80f};

    // 272W load: 1224 * 60 / 272 = 270 minutes
    uint32_t est272W = VoltWatch::OutageStateMachine::calculateBatteryRuntimeMinutes(config, 272.0f);
    ASSERT_EQ(270u, est272W);

    // 612W load: 1224 * 60 / 612 = 120 minutes (2 hours)
    uint32_t est612W = VoltWatch::OutageStateMachine::calculateBatteryRuntimeMinutes(config, 612.0f);
    ASSERT_EQ(120u, est612W);

    // 1224W load: 1224 * 60 / 1224 = 60 minutes (1 hour)
    uint32_t est1224W = VoltWatch::OutageStateMachine::calculateBatteryRuntimeMinutes(config, 1224.0f);
    ASSERT_EQ(60u, est1224W);

    // 0W or negative load: safe zero
    ASSERT_EQ(0u, VoltWatch::OutageStateMachine::calculateBatteryRuntimeMinutes(config, 0.0f));
    ASSERT_EQ(0u, VoltWatch::OutageStateMachine::calculateBatteryRuntimeMinutes(config, -50.0f));

    // Custom configuration (24V 200Ah @ 90% eff, 80% DoD = 3456 Wh)
    // At 576W: 3456 * 60 / 576 = 360 minutes (6 hours)
    const VoltWatch::BatteryConfig customConfig = {24.0f, 200.0f, 0.90f, 0.80f};
    ASSERT_EQ(360u, VoltWatch::OutageStateMachine::calculateBatteryRuntimeMinutes(customConfig, 576.0f));
}

TEST_CASE(OutageStateMachine, AcknowledgeRecovery) {
    VoltWatch::OutageStateMachine sm(170.0f, 80.0f, 1); // 1-cycle debounce for quick check
    sm.init(230.0f, true, 0.0f, 0.0f, 1);

    sm.update(0.0f, true, 0.0f, 0.0f, 2);
    ASSERT_TRUE(sm.isOutage());

    sm.update(230.0f, true, 0.0f, 0.0f, 3);
    ASSERT_TRUE(sm.isRecovered());

    sm.acknowledgeRecovery();
    ASSERT_TRUE(sm.isNormal());
    ASSERT_FALSE(sm.hasStateChanged());
}

TEST_CASE(OutageStateMachine, BatteryTareLossAndAutonomyCeiling) {
    // 12V 150Ah @ 85% eff and 80% DoD = 1224 Wh usable AC
    // tareLoss = 20W, floor = 25W, ceiling = 2880 minutes (48 hours)
    const VoltWatch::BatteryConfig configWithTare = {
        12.0f, 150.0f, 0.85f, 0.80f, 20.0f, 25.0f, 2880
    };

    // 0W load: effective load clamped to 25W floor.
    // Uncapped would be 1224 * 60 / 25 = 2937.6 min, but capped to 2880 min ceiling!
    uint32_t est0W = VoltWatch::OutageStateMachine::calculateBatteryRuntimeMinutes(configWithTare, 0.0f);
    ASSERT_EQ(2880u, est0W);

    // 10W load: effective load = 10W + 20W tare = 30W.
    // 1224 * 60 / 30 = 2448 minutes (40.8 hours)
    uint32_t est10W = VoltWatch::OutageStateMachine::calculateBatteryRuntimeMinutes(configWithTare, 10.0f);
    ASSERT_EQ(2448u, est10W);

    // 180W load: effective load = 180W + 20W tare = 200W.
    // 1224 * 60 / 200 = 367.2 -> 367 minutes (approx 6.1 hours)
    uint32_t est180W = VoltWatch::OutageStateMachine::calculateBatteryRuntimeMinutes(configWithTare, 180.0f);
    ASSERT_EQ(367u, est180W);
}
