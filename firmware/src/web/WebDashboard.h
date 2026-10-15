#ifndef VOLTWATCH_WEB_DASHBOARD_H
#define VOLTWATCH_WEB_DASHBOARD_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "../../include/types.h"
#include "../../include/config.h"

#if defined(ARDUINO) || defined(ESP32)
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#endif

namespace VoltWatch {

/**
 * @brief Embedded Asynchronous Web Server, REST API & Telemetry Pipeline
 * 
 * Provides:
 * - Direct REST JSON Endpoint: GET /api/status (exact schema matching requirements)
 * - REST Endpoint: GET /api/history (last 10 historical outages)
 * - Real-Time Server-Sent Events (SSE): /api/events (pushing status every 1000ms)
 * - Global CORS support for Headless Metrology & Single Page Applications (SPA)
 * - mDNS responder for http://voltwatch.local
 * - Lightweight JSON diagnostic payload served at / and /index.html
 * - Seamless desktop / macOS unit test mocking support
 */
class WebDashboard {
public:
    WebDashboard();
    ~WebDashboard();

    /**
     * @brief Initialize web server, register REST & SSE routes, and start mDNS responder
     * @param port HTTP port to listen on (default: 80)
     * @return true if initialized successfully
     */
    bool begin(uint16_t port = 80);

    /**
     * @brief Start listening on HTTP socket and initialize mDNS responder.
     * Deferring socket creation until Wi-Fi connects prevents tcp_alloc assertion in Core 3.x.
     */
    bool startListening();

    /**
     * @brief Stop web server and release resources
     */
    void stop();

    /**
     * @brief Update the cached real-time power system status
     * @param status Aggregated power metrology and outage state
     */
    void updateStatus(const PowerSystemStatus& status);

    /**
     * @brief Update cached historical outage records ring buffer
     * @param records Pointer to array of OutageRecord
     * @param count Number of records (capped at Config::OUTAGE_HISTORY_MAX)
     */
    void updateHistory(const OutageRecord* records, size_t count);

    /**
     * @brief Periodic background task (pushes SSE events every SSE_PUSH_INTERVAL_MS)
     * @param currentMillis Current microcontroller uptime in milliseconds
     */
    void update(uint32_t currentMillis);

    /**
     * @brief Immediately broadcast the latest status JSON to all connected SSE clients
     */
    void pushSseUpdate();

    /**
     * @brief Get current cached power system status
     */
    const PowerSystemStatus& getStatus() const { return m_status; }

    /**
     * @brief Get pointer to cached outage history records
     */
    const OutageRecord* getHistory() const { return m_history; }

    /**
     * @brief Get number of cached outage history records
     */
    size_t getHistoryCount() const { return m_historyCount; }

    /**
     * @brief Check if web server is running
     */
    bool isRunning() const { return m_running; }

    /**
     * @brief Check if mDNS responder is active
     */
    bool isMdnsActive() const { return m_mdnsActive; }

    /**
     * @brief Get HTTP port
     */
    uint16_t getPort() const { return m_port; }

    // =========================================================================
    // Direct REST JSON Serialization Utilities
    // =========================================================================

    /**
     * @brief Serialize /api/status JSON payload into destination buffer
     * Matches exact schema specified in docs/requirements.md
     * @param status Current power system status
     * @param buffer Destination character buffer
     * @param bufferSize Capacity of destination buffer
     * @return Number of characters written (excluding null terminator)
     */
    static size_t serializeStatusJson(const PowerSystemStatus& status, char* buffer, size_t bufferSize);

    /**
     * @brief Build /api/status JSON payload as std::string
     */
    static std::string buildStatusJson(const PowerSystemStatus& status);

    /**
     * @brief Serialize /api/history JSON array payload into destination buffer
     * @param records Array of OutageRecord structures
     * @param count Number of records to serialize
     * @param buffer Destination character buffer
     * @param bufferSize Capacity of destination buffer
     * @return Number of characters written (excluding null terminator)
     */
    static size_t serializeHistoryJson(const OutageRecord* records, size_t count, char* buffer, size_t bufferSize);

    /**
     * @brief Build /api/history JSON array payload as std::string
     */
    static std::string buildHistoryJson(const OutageRecord* records, size_t count);

    /**
     * @brief Overloaded helper for std::vector<OutageRecord>
     */
    static std::string buildHistoryJson(const std::vector<OutageRecord>& records);


#if !defined(ARDUINO) && !defined(ESP32)
    // =========================================================================
    // Mock / Simulated Request Handling for Native macOS & Unit Tests
    // =========================================================================
    struct MockResponse {
        int statusCode{200};
        std::string contentType{"text/plain"};
        std::string body;
        std::vector<std::pair<std::string, std::string>> headers;

        std::string getHeader(const std::string& name) const {
            for (const auto& h : headers) {
                if (h.first == name) return h.second;
            }
            return "";
        }
    };

    /**
     * @brief Dispatch a simulated HTTP request through the dashboard routing pipeline
     * @param method HTTP method ("GET", "POST", etc.)
     * @param path Request URI path (e.g., "/api/status", "/api/history", "/")
     * @return MockResponse containing HTTP status code, Content-Type, and body
     */
    MockResponse handleSimulatedRequest(const std::string& method, const std::string& path);
#endif

private:
    PowerSystemStatus m_status{};
    OutageRecord m_history[Config::OUTAGE_HISTORY_MAX]{};
    size_t m_historyCount{0};
    uint32_t m_lastSsePushTime{0};
    bool m_running{false};
    bool m_mdnsActive{false};
    uint16_t m_port{80};

#if defined(ARDUINO) || defined(ESP32)
    AsyncWebServer* m_server{nullptr};
    AsyncEventSource* m_events{nullptr};
#endif
};

} // namespace VoltWatch

#endif // VOLTWATCH_WEB_DASHBOARD_H
