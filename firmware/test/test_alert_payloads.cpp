#include "test_framework.h"
#include "../src/alerts/SMTPAlertDispatcher.h"
#include <cstring>
#include <string>

TEST_CASE(SMTPAlertDispatcher, OutagePayloadPlainText) {
    VoltWatch::SMTPAlertDispatcher dispatcher;

    VoltWatch::OutageAlertParams params;
    params.timestampEpoch = 1776250000;
    params.gridVoltageBeforeTrip = 232.4f;
    params.loadWatts = 345.2f;
    params.currentAmps = 1.54f;
    params.powerFactor = 0.96f;
    params.estBatteryRuntimeMin = 270;

    VoltWatch::AlertMessage msg = dispatcher.generateOutagePayload(params);

    // Subject verification
    ASSERT_EQ(std::string("🚨 [ALERT] Power Outage Detected - Grid Down"), msg.subject);

    // Plain text content checks
    const std::string& text = msg.textBody;
    ASSERT_TRUE(text.find("🚨 VOLTWATCH ALERT: POWER OUTAGE DETECTED") != std::string::npos);
    ASSERT_TRUE(text.find("IST") != std::string::npos);
    ASSERT_TRUE(text.find("232.4 V") != std::string::npos);
    ASSERT_TRUE(text.find("345.2 W") != std::string::npos);
    ASSERT_TRUE(text.find("1.54 A") != std::string::npos);
    ASSERT_TRUE(text.find("0.96") != std::string::npos);
    ASSERT_TRUE(text.find("270 min") != std::string::npos);
    ASSERT_TRUE(text.find("4h 30m") != std::string::npos);
}

TEST_CASE(SMTPAlertDispatcher, OutagePayloadHtml) {
    VoltWatch::SMTPAlertDispatcher dispatcher;

    VoltWatch::OutageAlertParams params;
    params.timestampEpoch = 1776250000;
    params.gridVoltageBeforeTrip = 232.4f;
    params.loadWatts = 345.2f;
    params.currentAmps = 1.54f;
    params.powerFactor = 0.96f;
    params.estBatteryRuntimeMin = 270;

    VoltWatch::AlertMessage msg = dispatcher.generateOutagePayload(params);

    const std::string& html = msg.htmlBody;
    ASSERT_TRUE(html.find("<!DOCTYPE html>") != std::string::npos);
    ASSERT_TRUE(html.find("🚨 Power Outage Detected") != std::string::npos);
    ASSERT_TRUE(html.find("232.4 V") != std::string::npos);
    ASSERT_TRUE(html.find("345.2 W") != std::string::npos);
    ASSERT_TRUE(html.find("1.54 A") != std::string::npos);
    ASSERT_TRUE(html.find("0.96") != std::string::npos);
    ASSERT_TRUE(html.find("270 min") != std::string::npos);
    ASSERT_TRUE(html.find("IST") != std::string::npos);
    ASSERT_TRUE(html.find("#D32F2F") != std::string::npos);
}

TEST_CASE(SMTPAlertDispatcher, RestorationPayloadPlainText) {
    VoltWatch::SMTPAlertDispatcher dispatcher;

    VoltWatch::RestorationAlertParams params;
    params.timestampEpoch = 1776254462;
    params.durationSeconds = 4462; // 1h 14m 22s
    params.energyConsumedKwh = 0.450f;
    params.restoredGridVoltage = 235.1f;
    params.restoredGridFrequency = 50.0f;

    VoltWatch::AlertMessage msg = dispatcher.generateRestorationPayload(params);

    // Subject verification
    ASSERT_EQ(std::string("✅ [RESTORED] Grid Power Resumed"), msg.subject);

    // Plain text content checks
    const std::string& text = msg.textBody;
    ASSERT_TRUE(text.find("✅ VOLTWATCH ALERT: GRID POWER RESUMED") != std::string::npos);
    ASSERT_TRUE(text.find("IST") != std::string::npos);
    ASSERT_TRUE(text.find("1h 14m 22s") != std::string::npos);
    ASSERT_TRUE(text.find("0.450 kWh") != std::string::npos);
    ASSERT_TRUE(text.find("235.1 V") != std::string::npos);
    ASSERT_TRUE(text.find("50.0 Hz") != std::string::npos);
}

TEST_CASE(SMTPAlertDispatcher, RestorationPayloadHtml) {
    VoltWatch::SMTPAlertDispatcher dispatcher;

    VoltWatch::RestorationAlertParams params;
    params.timestampEpoch = 1776254462;
    params.durationSeconds = 4462;
    params.energyConsumedKwh = 0.450f;
    params.restoredGridVoltage = 235.1f;
    params.restoredGridFrequency = 50.0f;

    VoltWatch::AlertMessage msg = dispatcher.generateRestorationPayload(params);

    const std::string& html = msg.htmlBody;
    ASSERT_TRUE(html.find("<!DOCTYPE html>") != std::string::npos);
    ASSERT_TRUE(html.find("✅ Grid Power Resumed") != std::string::npos);
    ASSERT_TRUE(html.find("1h 14m 22s") != std::string::npos);
    ASSERT_TRUE(html.find("0.450 kWh") != std::string::npos);
    ASSERT_TRUE(html.find("235.1 V") != std::string::npos);
    ASSERT_TRUE(html.find("50.0 Hz") != std::string::npos);
    ASSERT_TRUE(html.find("IST") != std::string::npos);
    ASSERT_TRUE(html.find("#2E7D32") != std::string::npos);
}

TEST_CASE(SMTPAlertDispatcher, MultiRecipientRouting) {
    VoltWatch::SMTPAlertDispatcher dispatcher;

    size_t expectedCount = 0;
    std::string r1 = SMTP_RECIPIENT_1;
    std::string r2 = SMTP_RECIPIENT_2;
    std::string r3 = SMTP_RECIPIENT_3;
    if (!r1.empty()) expectedCount++;
    if (!r2.empty()) expectedCount++;
    if (!r3.empty()) expectedCount++;

    ASSERT_EQ(expectedCount, dispatcher.getRecipientCount());
    ASSERT_TRUE(expectedCount >= 1);

    const auto& rec = dispatcher.getRecipients();
    size_t idx = 0;
    if (!r1.empty()) ASSERT_EQ(r1, rec[idx++].email);
    if (!r2.empty()) ASSERT_EQ(r2, rec[idx++].email);
    if (!r3.empty()) ASSERT_EQ(r3, rec[idx++].email);

    // Custom recipient manipulation
    dispatcher.clearRecipients();
    ASSERT_EQ(size_t(0), dispatcher.getRecipientCount());

    dispatcher.addRecipient("Ops Lead", "ops@example.com");
    ASSERT_EQ(size_t(1), dispatcher.getRecipientCount());
    ASSERT_EQ(std::string("ops@example.com"), dispatcher.getRecipients()[0].email);

    dispatcher.resetDefaultRecipients();
    ASSERT_EQ(expectedCount, dispatcher.getRecipientCount());
}

TEST_CASE(SMTPAlertDispatcher, MockDispatchExecution) {
    VoltWatch::SMTPAlertDispatcher dispatcher;
    dispatcher.setMockMode(true);
    ASSERT_TRUE(dispatcher.isMockMode());

    VoltWatch::OutageAlertParams outage;
    outage.timestampEpoch = 1776250000;
    outage.gridVoltageBeforeTrip = 228.0f;
    outage.loadWatts = 420.0f;
    outage.currentAmps = 1.9f;
    outage.powerFactor = 0.95f;
    outage.estBatteryRuntimeMin = 210;

    bool ok1 = dispatcher.sendOutageAlert(outage);
    ASSERT_TRUE(ok1);
    ASSERT_EQ(uint32_t(1), dispatcher.getSentAlertCount());
    ASSERT_EQ(std::string("🚨 [ALERT] Power Outage Detected - Grid Down"), dispatcher.getLastSentAlert().subject);
    ASSERT_TRUE(dispatcher.getLastSentAlert().sentSuccessfully);

    VoltWatch::RestorationAlertParams restored;
    restored.timestampEpoch = 1776253600;
    restored.durationSeconds = 3600;
    restored.energyConsumedKwh = 0.42f;
    restored.restoredGridVoltage = 230.5f;
    restored.restoredGridFrequency = 50.0f;

    bool ok2 = dispatcher.sendRestorationAlert(restored);
    ASSERT_TRUE(ok2);
    ASSERT_EQ(uint32_t(2), dispatcher.getSentAlertCount());
    ASSERT_EQ(std::string("✅ [RESTORED] Grid Power Resumed"), dispatcher.getLastSentAlert().subject);
}

TEST_CASE(SMTPAlertDispatcher, DurationAndTimestampFormatters) {
    ASSERT_EQ(std::string("0h 0m 0s"), VoltWatch::SMTPAlertDispatcher::formatDuration(0));
    ASSERT_EQ(std::string("0h 1m 5s"), VoltWatch::SMTPAlertDispatcher::formatDuration(65));
    ASSERT_EQ(std::string("1h 14m 22s"), VoltWatch::SMTPAlertDispatcher::formatDuration(4462));
    ASSERT_EQ(std::string("24h 0m 0s"), VoltWatch::SMTPAlertDispatcher::formatDuration(86400));

    std::string ts = VoltWatch::SMTPAlertDispatcher::formatTimestampIST(1776250000);
    ASSERT_TRUE(ts.find("IST") != std::string::npos);
    ASSERT_TRUE(ts.length() >= 19);
}

TEST_CASE(SMTPAlertDispatcher, OfflineQueueAndFifoFlush) {
    VoltWatch::SMTPAlertDispatcher dispatcher;
    dispatcher.setMockMode(true);
    dispatcher.setMockNetworkOnline(false); // Simulate router power loss / Wi-Fi disconnected

    VoltWatch::OutageAlertParams outage;
    outage.timestampEpoch = 1776250000;
    outage.gridVoltageBeforeTrip = 230.0f;
    outage.loadWatts = 300.0f;
    outage.currentAmps = 1.3f;
    outage.powerFactor = 0.95f;
    outage.estBatteryRuntimeMin = 240;
    outage.isNtpSynced = true;

    VoltWatch::RestorationAlertParams restored;
    restored.timestampEpoch = 1776251800;
    restored.durationSeconds = 1800;
    restored.energyConsumedKwh = 0.15f;
    restored.restoredGridVoltage = 232.0f;
    restored.restoredGridFrequency = 50.0f;
    restored.isNtpSynced = true;

    // Both alerts queued while offline
    dispatcher.sendOutageAlert(outage);
    dispatcher.sendRestorationAlert(restored);

    ASSERT_EQ(size_t(2), dispatcher.getPendingQueueCount());
    ASSERT_EQ(uint32_t(0), dispatcher.getSentAlertCount()); // 0 sent while offline

    // Router powers back on: Wi-Fi restored
    dispatcher.setMockNetworkOnline(true);

    // Flush #1: Outage alert sent first (FIFO)
    bool ok1 = dispatcher.processNextPendingAlert();
    ASSERT_TRUE(ok1);
    ASSERT_EQ(uint32_t(1), dispatcher.getSentAlertCount());
    ASSERT_EQ(std::string("🚨 [ALERT] Power Outage Detected - Grid Down"), dispatcher.getLastSentAlert().subject);
    ASSERT_EQ(size_t(1), dispatcher.getPendingQueueCount());

    // Flush #2: Restoration alert sent second
    bool ok2 = dispatcher.processNextPendingAlert();
    ASSERT_TRUE(ok2);
    ASSERT_EQ(uint32_t(2), dispatcher.getSentAlertCount());
    ASSERT_EQ(std::string("✅ [RESTORED] Grid Power Resumed"), dispatcher.getLastSentAlert().subject);
    ASSERT_EQ(size_t(0), dispatcher.getPendingQueueCount());
}

TEST_CASE(SMTPAlertDispatcher, ColdBootNtpUnsyncedGuard) {
    // Small uptime timestamp (e.g. 142s uptime on cold boot during blackout)
    uint32_t coldBootUptime = 142;
    ASSERT_FALSE(VoltWatch::SMTPAlertDispatcher::isNtpSynced(coldBootUptime));

    std::string unsyncedTs = VoltWatch::SMTPAlertDispatcher::formatTimestampIST(coldBootUptime, false);
    ASSERT_TRUE(unsyncedTs.find("Uptime: 142s (NTP Unsynced)") != std::string::npos);
    ASSERT_TRUE(unsyncedTs.find("1970") == std::string::npos); // Never print 1970

    // Outage payload generation with unsynced clock
    VoltWatch::SMTPAlertDispatcher dispatcher;
    VoltWatch::OutageAlertParams params;
    params.timestampEpoch = coldBootUptime;
    params.isNtpSynced = false;
    params.gridVoltageBeforeTrip = 0.0f;
    params.loadWatts = 200.0f;
    params.estBatteryRuntimeMin = 180;

    VoltWatch::AlertMessage msg = dispatcher.generateOutagePayload(params);
    ASSERT_TRUE(msg.textBody.find("Uptime: 142s (NTP Unsynced)") != std::string::npos);
    ASSERT_TRUE(msg.textBody.find("1970") == std::string::npos);
    ASSERT_TRUE(msg.htmlBody.find("Uptime: 142s (NTP Unsynced)") != std::string::npos);
    ASSERT_TRUE(msg.htmlBody.find("1970") == std::string::npos);
}
