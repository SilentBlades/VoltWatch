#ifndef VOLTWATCH_SMTP_ALERT_DISPATCHER_H
#define VOLTWATCH_SMTP_ALERT_DISPATCHER_H

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>
#include <deque>

#include "include/config.h"
#include "include/types.h"

#if defined(ARDUINO) || defined(ESP32)
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#if __has_include(<ESP_Mail_Client.h>)
#include <ESP_Mail_Client.h>
#endif
#endif

namespace VoltWatch {

/**
 * @brief Parameters for formatting an Outage Alert email
 */
struct OutageAlertParams {
    uint32_t timestampEpoch{0};         ///< Outage trip epoch timestamp in seconds
    float gridVoltageBeforeTrip{0.0f};  ///< Grid voltage right before trip occurred (V)
    float loadWatts{0.0f};              ///< Inverter active load (W)
    float currentAmps{0.0f};            ///< Inverter load current (A)
    float powerFactor{0.0f};            ///< Inverter power factor (0.00 - 1.00)
    uint32_t estBatteryRuntimeMin{0};   ///< Estimated battery backup runtime (minutes)
    bool isNtpSynced{false};            ///< True if timestampEpoch is valid synchronized epoch
};

/**
 * @brief Parameters for formatting a Restoration Alert email
 */
struct RestorationAlertParams {
    uint32_t timestampEpoch{0};         ///< Restoration epoch timestamp in seconds
    uint32_t durationSeconds{0};        ///< Total outage duration in seconds
    float energyConsumedKwh{0.0f};      ///< Battery energy consumed during outage (kWh)
    float restoredGridVoltage{0.0f};    ///< Grid line voltage upon recovery (V)
    float restoredGridFrequency{0.0f};  ///< Grid line frequency upon recovery (Hz)
    bool isNtpSynced{false};            ///< True if timestampEpoch is valid synchronized epoch
};

/**
 * @brief Recipient email address and display name
 */
struct EmailRecipient {
    std::string name;
    std::string email;
};

/**
 * @brief Aggregated alert message with subject, bodies, and target recipients
 */
struct AlertMessage {
    std::string subject;
    std::string textBody;
    std::string htmlBody;
    std::vector<EmailRecipient> recipients;
    bool sentSuccessfully{false};
    uint8_t retryCount{0};
};

/**
 * @brief SMTPS client configuration
 */
struct SmtpConfig {
    std::string host{SMTP_HOST};
    uint16_t port{SMTP_PORT};
    std::string senderName{SMTP_SENDER_NAME};
    std::string senderEmail{SMTP_SENDER_EMAIL};
    std::string appPassword{SMTP_APP_PASSWORD};
};

/**
 * @brief Direct SMTPS Alert Dispatcher with FreeRTOS Core 0 Background Worker & Offline FIFO Queue.
 *
 * Architecture Highlights:
 * 1. Dedicated FreeRTOS Background Task: On ESP32, all mbedTLS handshakes and SMTP transmissions
 *    run on Core 0 ("smtp_task", 8KB stack). loop() on Core 1 pushes via FreeRTOS queue with 0ms delay!
 * 2. Offline FIFO Alert Queue: If Wi-Fi/router drops during an outage, Outage alerts are retained
 *    in queue. When Wi-Fi restores, the queue flushes FIFO: Outage alert sent first, then Recovery alert!
 * 3. NTP Cold Boot Guard: Validates clock sync status before printing timestamps, preventing 1970 artifacts.
 */
class SMTPAlertDispatcher {
public:
    explicit SMTPAlertDispatcher(const SmtpConfig& config = SmtpConfig{});
    ~SMTPAlertDispatcher();

    /**
     * @brief Initialize dispatcher, recipient routing, offline queues, and background task.
     */
    void init();

    /**
     * @brief Setup NTP time synchronization for IST timezone (UTC+5:30)
     */
    void initNtp(const char* ntpServer = VoltWatch::Config::NTP_SERVER,
                 long gmtOffsetSec = VoltWatch::Config::TIMEZONE_OFFSET_SEC,
                 int daylightOffsetSec = VoltWatch::Config::DAYLIGHT_OFFSET_SEC);

    // --- Recipient Management ---
    void addRecipient(const std::string& name, const std::string& email);
    void clearRecipients();
    void resetDefaultRecipients();
    const std::vector<EmailRecipient>& getRecipients() const { return m_recipients; }
    size_t getRecipientCount() const { return m_recipients.size(); }

    // --- Payload Generation (Subject, Plain Text & HTML) ---
    AlertMessage generateOutagePayload(const OutageAlertParams& params) const;
    AlertMessage generateRestorationPayload(const RestorationAlertParams& params) const;

    AlertMessage generateOutagePayload(const OutageRecord& record, const PowerSystemStatus& status) const;
    AlertMessage generateRestorationPayload(const OutageRecord& record, const PowerSystemStatus& status) const;

    // --- Non-Blocking Dispatch API (Returns immediately, 0ms stall to loop()) ---
    bool queueAlert(const AlertMessage& alert);
    bool sendOutageAlert(const OutageAlertParams& params);
    bool sendRestorationAlert(const RestorationAlertParams& params);
    bool sendAlert(const AlertMessage& alert);

    // --- Offline Queue Processing & FreeRTOS Worker ---
    bool processNextPendingAlert();
    size_t getPendingQueueCount() const { return m_pendingQueue.size(); }
    void clearPendingQueue() { m_pendingQueue.clear(); }

    // --- Clock & NTP Guard Utilities ---
    static bool isNtpSynced(uint32_t epochSeconds);
    static std::string formatTimestampIST(uint32_t epochSeconds, bool isSynced = false);
    static std::string formatDuration(uint32_t durationSeconds);
    static std::string getOutageSubject();
    static std::string getRestorationSubject();

    // --- Mocking & Test Diagnostics ---
    void setMockMode(bool enable) { m_mockMode = enable; }
    bool isMockMode() const { return m_mockMode; }
    void setMockNetworkOnline(bool online) { m_mockNetworkOnline = online; }
    bool isMockNetworkOnline() const { return m_mockNetworkOnline; }

    const AlertMessage& getLastSentAlert() const { return m_lastSentAlert; }
    uint32_t getSentAlertCount() const { return m_sentAlertCount; }
    void resetSentAlertCount() { m_sentAlertCount = 0; }

private:
    bool executeSmtpTransport(const AlertMessage& alert);

#if defined(ARDUINO) || defined(ESP32)
    static void smtpTaskWorker(void* pvParameters);
    QueueHandle_t m_freeRtosQueue{nullptr};
    TaskHandle_t m_taskHandle{nullptr};
#endif

    SmtpConfig m_config;
    std::vector<EmailRecipient> m_recipients;
    std::deque<AlertMessage> m_pendingQueue;
    bool m_mockMode{false};
    bool m_mockNetworkOnline{true};
    uint32_t m_sentAlertCount{0};
    AlertMessage m_lastSentAlert;
};

} // namespace VoltWatch

#endif // VOLTWATCH_SMTP_ALERT_DISPATCHER_H
