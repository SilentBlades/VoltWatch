#include "test_framework.h"
#include "../src/alerts/AlertManager.h"

static const VoltWatch::VoltageThresholds kTestThresholds = {
    12.0f,  // Nominal
    10.5f,  // CritLow
    11.4f,  // WarnLow
    12.8f,  // WarnHigh
    14.4f,  // CritHigh
    0.20f   // Hysteresis
};

TEST_CASE(AlertManager, NormalRange) {
    VoltWatch::AlertManager manager(kTestThresholds, 1); // 1 sample debounce for direct tests
    VoltWatch::AlertLevel level = manager.update(12.0f);
    ASSERT_EQ(static_cast<int>(VoltWatch::AlertLevel::NORMAL), static_cast<int>(level));
    ASSERT_FALSE(manager.isCritical());
    ASSERT_FALSE(manager.isWarning());
}

TEST_CASE(AlertManager, WarningLowDetection) {
    VoltWatch::AlertManager manager(kTestThresholds, 1);
    VoltWatch::AlertLevel level = manager.update(11.2f);
    ASSERT_EQ(static_cast<int>(VoltWatch::AlertLevel::WARNING_LOW), static_cast<int>(level));
    ASSERT_TRUE(manager.isWarning());
    ASSERT_FALSE(manager.isCritical());
}

TEST_CASE(AlertManager, CriticalUnderVoltDetection) {
    VoltWatch::AlertManager manager(kTestThresholds, 1);
    VoltWatch::AlertLevel level = manager.update(10.0f);
    ASSERT_EQ(static_cast<int>(VoltWatch::AlertLevel::CRITICAL_UNDERVOLT), static_cast<int>(level));
    ASSERT_TRUE(manager.isCritical());
}

TEST_CASE(AlertManager, CriticalOverVoltDetection) {
    VoltWatch::AlertManager manager(kTestThresholds, 1);
    VoltWatch::AlertLevel level = manager.update(14.8f);
    ASSERT_EQ(static_cast<int>(VoltWatch::AlertLevel::CRITICAL_OVERVOLT), static_cast<int>(level));
    ASSERT_TRUE(manager.isCritical());
}

TEST_CASE(AlertManager, HysteresisBehavior) {
    VoltWatch::AlertManager manager(kTestThresholds, 1);
    // Enter critical over-voltage (threshold is 14.4V)
    manager.update(14.5f);
    ASSERT_EQ(static_cast<int>(VoltWatch::AlertLevel::CRITICAL_OVERVOLT), static_cast<int>(manager.getCurrentLevel()));

    // Drop to 14.3V (below 14.4V, but within 0.2V hysteresis: 14.4 - 0.2 = 14.2V)
    // Should REMAIN CRITICAL_OVERVOLT due to hysteresis!
    manager.update(14.3f);
    ASSERT_EQ(static_cast<int>(VoltWatch::AlertLevel::CRITICAL_OVERVOLT), static_cast<int>(manager.getCurrentLevel()));

    // Drop below 14.2V -> should transition to WARNING_HIGH (14.15V is >= 12.8V)
    manager.update(14.15f);
    ASSERT_EQ(static_cast<int>(VoltWatch::AlertLevel::WARNING_HIGH), static_cast<int>(manager.getCurrentLevel()));
}

TEST_CASE(AlertManager, DebouncingFilter) {
    // Require 3 consecutive samples to confirm transition
    VoltWatch::AlertManager manager(kTestThresholds, 3);
    manager.update(12.0f); // Sample 1: normal
    ASSERT_EQ(static_cast<int>(VoltWatch::AlertLevel::NORMAL), static_cast<int>(manager.getCurrentLevel()));

    // Transient spike: 1st sample
    manager.update(15.0f);
    ASSERT_EQ(static_cast<int>(VoltWatch::AlertLevel::NORMAL), static_cast<int>(manager.getCurrentLevel()));
    ASSERT_FALSE(manager.hasStateChanged());

    // 2nd consecutive spike sample
    manager.update(15.0f);
    ASSERT_EQ(static_cast<int>(VoltWatch::AlertLevel::NORMAL), static_cast<int>(manager.getCurrentLevel()));

    // 3rd consecutive spike sample -> state MUST change now
    manager.update(15.0f);
    ASSERT_EQ(static_cast<int>(VoltWatch::AlertLevel::CRITICAL_OVERVOLT), static_cast<int>(manager.getCurrentLevel()));
    ASSERT_TRUE(manager.hasStateChanged());
}
