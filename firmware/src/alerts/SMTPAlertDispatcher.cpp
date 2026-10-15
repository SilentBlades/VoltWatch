#include "SMTPAlertDispatcher.h"

#include <cstdio>
#include <ctime>
#include <sstream>
#include <iomanip>

#if defined(ARDUINO) || defined(ESP32)
#include <Arduino.h>
#include <time.h>
#include <WiFi.h>
#if __has_include(<esp_sntp.h>)
#include <esp_sntp.h>
#endif
#endif

namespace VoltWatch {

SMTPAlertDispatcher::SMTPAlertDispatcher(const SmtpConfig& config)
    : m_config(config) {
    resetDefaultRecipients();
}

SMTPAlertDispatcher::~SMTPAlertDispatcher() {
#if defined(ARDUINO) || defined(ESP32)
    if (m_taskHandle != nullptr) {
        vTaskDelete(m_taskHandle);
        m_taskHandle = nullptr;
    }
    if (m_freeRtosQueue != nullptr) {
        vQueueDelete(m_freeRtosQueue);
        m_freeRtosQueue = nullptr;
    }
#endif
}

#if defined(ARDUINO) || defined(ESP32)
void SMTPAlertDispatcher::smtpTaskWorker(void* pvParameters) {
    SMTPAlertDispatcher* self = static_cast<SMTPAlertDispatcher*>(pvParameters);
    while (true) {
        if (self->getPendingQueueCount() > 0) {
            // Process queued alerts in FIFO order when network is available
            bool success = self->processNextPendingAlert();
            if (!success) {
                // If offline or dispatch failed, back off for 3 seconds before retrying
                vTaskDelay(pdMS_TO_TICKS(3000));
            } else {
                // Short inter-message delay between flushes
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        } else {
            // Idle wait when queue is empty
            vTaskDelay(pdMS_TO_TICKS(250));
        }
    }
}
#endif

void SMTPAlertDispatcher::init() {
    resetDefaultRecipients();
    initNtp();

#if defined(ARDUINO) || defined(ESP32)
    if (m_freeRtosQueue == nullptr) {
        m_freeRtosQueue = xQueueCreate(VoltWatch::Config::SMTP_QUEUE_CAPACITY, sizeof(uint8_t));
    }
    if (m_taskHandle == nullptr) {
        xTaskCreatePinnedToCore(
            smtpTaskWorker,
            "smtp_task",
            VoltWatch::Config::SMTP_TASK_STACK_SIZE,
            this,
            VoltWatch::Config::SMTP_TASK_PRIORITY,
            &m_taskHandle,
            VoltWatch::Config::SMTP_TASK_CORE
        );
    }
#endif
}

void SMTPAlertDispatcher::initNtp(const char* ntpServer, long gmtOffsetSec, int daylightOffsetSec) {
#if defined(ARDUINO) || defined(ESP32)
    configTime(gmtOffsetSec, daylightOffsetSec, ntpServer);
#else
    (void)ntpServer;
    (void)gmtOffsetSec;
    (void)daylightOffsetSec;
#endif
}

void SMTPAlertDispatcher::addRecipient(const std::string& name, const std::string& email) {
    m_recipients.push_back({name, email});
}

void SMTPAlertDispatcher::clearRecipients() {
    m_recipients.clear();
}

void SMTPAlertDispatcher::resetDefaultRecipients() {
    m_recipients.clear();
    std::string r1 = SMTP_RECIPIENT_1;
    std::string r2 = SMTP_RECIPIENT_2;
    std::string r3 = SMTP_RECIPIENT_3;
    if (!r1.empty()) m_recipients.push_back({"Primary Alert", r1});
    if (!r2.empty()) m_recipients.push_back({"Secondary Alert", r2});
    if (!r3.empty()) m_recipients.push_back({"Tertiary Alert", r3});
}

std::string SMTPAlertDispatcher::getOutageSubject() {
    return "🚨 [ALERT] Power Outage Detected - Grid Down";
}

std::string SMTPAlertDispatcher::getRestorationSubject() {
    return "✅ [RESTORED] Grid Power Resumed";
}

bool SMTPAlertDispatcher::isNtpSynced(uint32_t epochSeconds) {
    if (epochSeconds >= 1700000000) { // Nov 2023 epoch boundary
        return true;
    }
#if defined(ARDUINO) || defined(ESP32)
    #if __has_include(<esp_sntp.h>)
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED && epochSeconds > 100000) {
        return true;
    }
    #endif
#endif
    return false;
}

std::string SMTPAlertDispatcher::formatTimestampIST(uint32_t epochSeconds, bool isSynced) {
    // Cold Boot NTP Guard: if unsynced, print relative uptime instead of 1970
    if (!isSynced && !isNtpSynced(epochSeconds)) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Uptime: %us (NTP Unsynced)", epochSeconds);
        return std::string(buf);
    }

    time_t istTime = static_cast<time_t>(epochSeconds + VoltWatch::Config::TIMEZONE_OFFSET_SEC);
    struct tm timeinfo{};
#if defined(_WIN32)
    gmtime_s(&timeinfo, &istTime);
#else
    gmtime_r(&istTime, &timeinfo);
#endif
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d IST",
                  timeinfo.tm_year + 1900,
                  timeinfo.tm_mon + 1,
                  timeinfo.tm_mday,
                  timeinfo.tm_hour,
                  timeinfo.tm_min,
                  timeinfo.tm_sec);
    return std::string(buf);
}

std::string SMTPAlertDispatcher::formatDuration(uint32_t durationSeconds) {
    uint32_t hours = durationSeconds / 3600;
    uint32_t mins = (durationSeconds % 3600) / 60;
    uint32_t secs = durationSeconds % 60;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%uh %um %us", hours, mins, secs);
    return std::string(buf);
}

AlertMessage SMTPAlertDispatcher::generateOutagePayload(const OutageAlertParams& params) const {
    AlertMessage msg;
    msg.subject = getOutageSubject();
    msg.recipients = m_recipients;

    std::string timestampStr = formatTimestampIST(params.timestampEpoch, params.isNtpSynced);

    // Format remaining runtime string
    uint32_t rtHours = params.estBatteryRuntimeMin / 60;
    uint32_t rtMins = params.estBatteryRuntimeMin % 60;
    char runtimeStr[64];
    std::snprintf(runtimeStr, sizeof(runtimeStr), "%u min (%uh %um)",
                  params.estBatteryRuntimeMin, rtHours, rtMins);

    // Plain text layout
    std::ostringstream txt;
    txt << "====================================================\n";
    txt << "🚨 VOLTWATCH ALERT: POWER OUTAGE DETECTED\n";
    txt << "====================================================\n\n";
    txt << "Status:       GRID DOWN (Running on Battery / Inverter)\n";
    txt << "Timestamp:    " << timestampStr << "\n";
    txt << "Pre-Trip V:   " << std::fixed << std::setprecision(1) << params.gridVoltageBeforeTrip << " V\n\n";
    txt << "--- Live Inverter Load Snapshot ---\n";
    txt << "Active Power: " << std::fixed << std::setprecision(1) << params.loadWatts << " W\n";
    txt << "Load Current: " << std::fixed << std::setprecision(2) << params.currentAmps << " A\n";
    txt << "Power Factor: " << std::fixed << std::setprecision(2) << params.powerFactor << "\n";
    txt << "Est. Autonomy:" << runtimeStr << "\n\n";
    txt << "VoltWatch Sentinel | Standalone Zero-Server Metrology\n";
    msg.textBody = txt.str();

    // HTML layout
    std::ostringstream html;
    html << "<!DOCTYPE html><html><head><meta charset='utf-8'></head>";
    html << "<body style='font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Helvetica,Arial,sans-serif;";
    html << "background-color:#121212;color:#E0E0E0;margin:0;padding:20px;'>";
    html << "<div style='max-width:600px;margin:0 auto;background:#1E1E1E;border-radius:12px;overflow:hidden;";
    html << "box-shadow:0 8px 24px rgba(0,0,0,0.5);border:1px solid #333;'>";
    html << "<div style='background:#D32F2F;padding:24px;text-align:center;'>";
    html << "<h1 style='color:#FFFFFF;margin:0;font-size:22px;letter-spacing:1px;'>🚨 Power Outage Detected</h1>";
    html << "<p style='color:rgba(255,255,255,0.85);margin:6px 0 0;font-size:14px;'>Grid Mains Failure - Running on Inverter</p>";
    html << "</div>";
    html << "<div style='padding:24px;'>";
    html << "<table style='width:100%;border-collapse:collapse;font-size:14px;'>";
    html << "<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Trip Timestamp</td>";
    html << "<td style='padding:10px 0;text-align:right;font-weight:bold;color:#FFFFFF;'>" << timestampStr << "</td></tr>";
    html << "<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Grid Voltage Pre-Trip</td>";
    html << "<td style='padding:10px 0;text-align:right;font-weight:bold;color:#FF5252;'>" << std::fixed << std::setprecision(1) << params.gridVoltageBeforeTrip << " V</td></tr>";
    html << "<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Inverter Active Load</td>";
    html << "<td style='padding:10px 0;text-align:right;font-weight:bold;color:#FFA726;'>" << std::fixed << std::setprecision(1) << params.loadWatts << " W</td></tr>";
    html << "<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Load Current</td>";
    html << "<td style='padding:10px 0;text-align:right;font-weight:bold;color:#FFFFFF;'>" << std::fixed << std::setprecision(2) << params.currentAmps << " A</td></tr>";
    html << "<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Power Factor</td>";
    html << "<td style='padding:10px 0;text-align:right;font-weight:bold;color:#FFFFFF;'>" << std::fixed << std::setprecision(2) << params.powerFactor << "</td></tr>";
    html << "<tr><td style='padding:12px 0 0;color:#9E9E9E;'>Est. Battery Autonomy</td>";
    html << "<td style='padding:12px 0 0;text-align:right;font-weight:bold;color:#66BB6A;font-size:16px;'>" << runtimeStr << "</td></tr>";
    html << "</table></div>";
    html << "<div style='background:#181818;padding:14px;text-align:center;font-size:12px;color:#757575;border-top:1px solid #282828;'>";
    html << "VoltWatch Standalone Edge Sentinel • Zero Server Dependency</div>";
    html << "</div></body></html>";
    msg.htmlBody = html.str();

    return msg;
}

AlertMessage SMTPAlertDispatcher::generateRestorationPayload(const RestorationAlertParams& params) const {
    AlertMessage msg;
    msg.subject = getRestorationSubject();
    msg.recipients = m_recipients;

    std::string timestampStr = formatTimestampIST(params.timestampEpoch, params.isNtpSynced);
    std::string durationStr = formatDuration(params.durationSeconds);

    // Plain text layout
    std::ostringstream txt;
    txt << "====================================================\n";
    txt << "✅ VOLTWATCH ALERT: GRID POWER RESUMED\n";
    txt << "====================================================\n\n";
    txt << "Status:       GRID RESTORED (Normal Operation)\n";
    txt << "Timestamp:    " << timestampStr << "\n";
    txt << "Duration:     " << durationStr << " (" << params.durationSeconds << " s)\n";
    txt << "Battery Drain:" << std::fixed << std::setprecision(3) << params.energyConsumedKwh << " kWh\n\n";
    txt << "--- Live Grid Mains Telemetry ---\n";
    txt << "Grid Voltage: " << std::fixed << std::setprecision(1) << params.restoredGridVoltage << " V\n";
    txt << "Frequency:    " << std::fixed << std::setprecision(1) << params.restoredGridFrequency << " Hz\n\n";
    txt << "VoltWatch Sentinel | Historical record preserved in NVS\n";
    msg.textBody = txt.str();

    // HTML layout
    std::ostringstream html;
    html << "<!DOCTYPE html><html><head><meta charset='utf-8'></head>";
    html << "<body style='font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Helvetica,Arial,sans-serif;";
    html << "background-color:#121212;color:#E0E0E0;margin:0;padding:20px;'>";
    html << "<div style='max-width:600px;margin:0 auto;background:#1E1E1E;border-radius:12px;overflow:hidden;";
    html << "box-shadow:0 8px 24px rgba(0,0,0,0.5);border:1px solid #333;'>";
    html << "<div style='background:#2E7D32;padding:24px;text-align:center;'>";
    html << "<h1 style='color:#FFFFFF;margin:0;font-size:22px;letter-spacing:1px;'>✅ Grid Power Resumed</h1>";
    html << "<p style='color:rgba(255,255,255,0.85);margin:6px 0 0;font-size:14px;'>Utility Power Restored - Running on Grid</p>";
    html << "</div>";
    html << "<div style='padding:24px;'>";
    html << "<table style='width:100%;border-collapse:collapse;font-size:14px;'>";
    html << "<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Restoration Time</td>";
    html << "<td style='padding:10px 0;text-align:right;font-weight:bold;color:#FFFFFF;'>" << timestampStr << "</td></tr>";
    html << "<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Total Outage Duration</td>";
    html << "<td style='padding:10px 0;text-align:right;font-weight:bold;color:#66BB6A;font-size:15px;'>" << durationStr << "</td></tr>";
    html << "<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Battery Energy Consumed</td>";
    html << "<td style='padding:10px 0;text-align:right;font-weight:bold;color:#FFA726;'>" << std::fixed << std::setprecision(3) << params.energyConsumedKwh << " kWh</td></tr>";
    html << "<tr style='border-bottom:1px solid #2C2C2C;'><td style='padding:10px 0;color:#9E9E9E;'>Restored Line Voltage</td>";
    html << "<td style='padding:10px 0;text-align:right;font-weight:bold;color:#4CAF50;'>" << std::fixed << std::setprecision(1) << params.restoredGridVoltage << " V</td></tr>";
    html << "<tr><td style='padding:10px 0;color:#9E9E9E;'>Restored Frequency</td>";
    html << "<td style='padding:10px 0;text-align:right;font-weight:bold;color:#FFFFFF;'>" << std::fixed << std::setprecision(1) << params.restoredGridFrequency << " Hz</td></tr>";
    html << "</table></div>";
    html << "<div style='background:#181818;padding:14px;text-align:center;font-size:12px;color:#757575;border-top:1px solid #282828;'>";
    html << "VoltWatch Standalone Edge Sentinel • Event Logged to NVS Flash</div>";
    html << "</div></body></html>";
    msg.htmlBody = html.str();

    return msg;
}

AlertMessage SMTPAlertDispatcher::generateOutagePayload(const OutageRecord& record, const PowerSystemStatus& status) const {
    OutageAlertParams params;
    params.timestampEpoch = record.startTimestamp;
    params.gridVoltageBeforeTrip = record.gridVoltageBeforeTrip;
    params.loadWatts = record.initialLoadWatts;
    params.currentAmps = status.inverter.current;
    params.powerFactor = status.inverter.powerFactor;
    params.estBatteryRuntimeMin = status.estBatteryRuntimeMinutes;
    params.isNtpSynced = record.isNtpSynced;
    return generateOutagePayload(params);
}

AlertMessage SMTPAlertDispatcher::generateRestorationPayload(const OutageRecord& record, const PowerSystemStatus& status) const {
    RestorationAlertParams params;
    params.timestampEpoch = record.endTimestamp;
    params.durationSeconds = record.durationSeconds;
    params.energyConsumedKwh = record.energyConsumedKwh;
    params.restoredGridVoltage = status.grid.voltage;
    params.restoredGridFrequency = status.grid.frequency;
    params.isNtpSynced = record.isNtpSynced;
    return generateRestorationPayload(params);
}

bool SMTPAlertDispatcher::queueAlert(const AlertMessage& alert) {
    if (m_pendingQueue.size() >= VoltWatch::Config::SMTP_QUEUE_CAPACITY) {
        // Drop oldest unconfirmed alert if queue reaches capacity
        m_pendingQueue.pop_front();
    }
    m_pendingQueue.push_back(alert);

    // In unit testing mock mode with network online, auto-process immediately
    if (m_mockMode && m_mockNetworkOnline) {
        return processNextPendingAlert();
    }

    return true;
}

bool SMTPAlertDispatcher::processNextPendingAlert() {
    if (m_pendingQueue.empty()) {
        return false;
    }

    // Check network connectivity
#if defined(ARDUINO) || defined(ESP32)
    if (WiFi.status() != WL_CONNECTED) {
        return false; // Leave in queue until Wi-Fi reconnects
    }
#else
    if (!m_mockNetworkOnline) {
        return false; // Simulated offline (router power loss)
    }
#endif

    AlertMessage& alert = m_pendingQueue.front();
    bool ok = executeSmtpTransport(alert);

    if (ok) {
        alert.sentSuccessfully = true;
        m_lastSentAlert = alert;
        m_sentAlertCount++;
        m_pendingQueue.pop_front();
        return true;
    } else {
        alert.retryCount++;
        return false;
    }
}

bool SMTPAlertDispatcher::executeSmtpTransport(const AlertMessage& alert) {
    if (m_mockMode) {
        return true;
    }

#if defined(ARDUINO) || defined(ESP32)
#if __has_include(<ESP_Mail_Client.h>)
    SMTPSession smtp;
    Session_Config config;
    config.server.host_name = m_config.host.c_str();
    config.server.port = m_config.port;
    config.login.email = m_config.senderEmail.c_str();
    config.login.password = m_config.appPassword.c_str();

    SMTP_Message message;
    message.sender.name = m_config.senderName.c_str();
    message.sender.email = m_config.senderEmail.c_str();
    message.subject = alert.subject.c_str();

    for (const auto& rec : alert.recipients) {
        if (!rec.email.empty()) {
            message.addRecipient(rec.name.c_str(), rec.email.c_str());
        }
    }

    message.text.content = alert.textBody.c_str();
    message.html.content = alert.htmlBody.c_str();

    if (!smtp.connect(&config)) {
        return false;
    }

    if (!MailClient.sendMail(&smtp, &message)) {
        return false;
    }

    return true;
#else
    (void)alert;
    return true;
#endif
#else
    (void)alert;
    return true;
#endif
}

bool SMTPAlertDispatcher::sendOutageAlert(const OutageAlertParams& params) {
    AlertMessage msg = generateOutagePayload(params);
    return queueAlert(msg);
}

bool SMTPAlertDispatcher::sendRestorationAlert(const RestorationAlertParams& params) {
    AlertMessage msg = generateRestorationPayload(params);
    return queueAlert(msg);
}

bool SMTPAlertDispatcher::sendAlert(const AlertMessage& alert) {
    return queueAlert(alert);
}

} // namespace VoltWatch
