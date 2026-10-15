#include "WebDashboard.h"
#include <stdio.h>
#include <string.h>
#include <algorithm>

#if defined(ARDUINO) || defined(ESP32)
#include <Arduino.h>
#endif

namespace VoltWatch {

WebDashboard::WebDashboard() {
    // Zero-initialize status and history
    m_status = {};
    m_historyCount = 0;
    m_running = false;
    m_mdnsActive = false;
    m_pendingStart = false;
    m_port = 80;
}

WebDashboard::~WebDashboard() {
    stop();
}

void WebDashboard::sendCorsHeaders() {
#if defined(ARDUINO) || defined(ESP32)
    if (m_server) {
        m_server->sendHeader("Access-Control-Allow-Origin", "*");
        m_server->sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
        m_server->sendHeader("Access-Control-Allow-Headers", "*");
        m_server->sendHeader("Access-Control-Allow-Private-Network", "true");
    }
#endif
}

bool WebDashboard::begin(uint16_t port) {
    m_port = port;

#if defined(ARDUINO) || defined(ESP32)
    stop(); // Ensure any prior server instance is torn down cleanly

    m_server = new WebServer(m_port);

    // Route: Diagnostic JSON payload for "/" and "/index.html"
    auto sendDiagnosticResponse = [this]() {
        this->sendCorsHeaders();
        this->m_server->send(200, "application/json", "{\"service\":\"VoltWatch\",\"status\":\"online\",\"api\":[\"/api/status\",\"/api/history\"]}");
    };

    m_server->on("/", HTTP_GET, sendDiagnosticResponse);
    m_server->on("/index.html", HTTP_GET, sendDiagnosticResponse);

    // Route: Direct REST JSON Endpoint GET /api/status (Thread-safe status snapshot)
    m_server->on("/api/status", HTTP_GET, [this]() {
        this->sendCorsHeaders();
        std::string json;
        {
            std::lock_guard<std::mutex> lock(this->m_dataMutex);
            json = buildStatusJson(this->m_status);
        }
        this->m_server->send(200, "application/json", json.c_str());
    });

    // Route: REST JSON Endpoint GET /api/history (Thread-safe history snapshot)
    m_server->on("/api/history", HTTP_GET, [this]() {
        this->sendCorsHeaders();
        std::string json;
        {
            std::lock_guard<std::mutex> lock(this->m_dataMutex);
            json = buildHistoryJson(this->m_history, this->m_historyCount);
        }
        this->m_server->send(200, "application/json", json.c_str());
    });

    // OPTIONS preflight handlers
    auto handleOptions = [this]() {
        this->sendCorsHeaders();
        this->m_server->send(204);
    };
    m_server->on("/api/status", HTTP_OPTIONS, handleOptions);
    m_server->on("/api/history", HTTP_OPTIONS, handleOptions);
    m_server->on("/", HTTP_OPTIONS, handleOptions);
    m_server->on("/index.html", HTTP_OPTIONS, handleOptions);

    // Handle HTTP OPTIONS preflight requests globally and catch-all 404
    m_server->onNotFound([this]() {
        this->sendCorsHeaders();
        if (this->m_server->method() == HTTP_OPTIONS) {
            this->m_server->send(204);
        } else {
            this->m_server->send(404, "application/json", "{\"error\":\"Not Found\"}");
        }
    });

#if defined(ARDUINO_EVENT_WIFI_STA_GOT_IP)
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        (void)event;
        (void)info;
        Serial.printf("[WebDashboard] Wi-Fi STA Got IP: %s. Scheduling WebServer activation on main loop...\n",
                      WiFi.localIP().toString().c_str());
        this->m_pendingStart = true;
    }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
#elif defined(SYSTEM_EVENT_STA_GOT_IP)
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        (void)event;
        (void)info;
        this->m_pendingStart = true;
    }, SYSTEM_EVENT_STA_GOT_IP);
#endif

#if defined(ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        (void)event;
        (void)info;
        this->m_pendingStart = false;
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
#elif defined(SYSTEM_EVENT_STA_DISCONNECTED)
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        (void)event;
        (void)info;
        this->m_pendingStart = false;
    }, SYSTEM_EVENT_STA_DISCONNECTED);
#endif

    // If WiFi is already connected during initialization, schedule start on main loop
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE) {
        m_pendingStart = true;
    }
#else
    // Simulated / native environment
    m_running = true;
    m_mdnsActive = true;
#endif

    return true;
}

bool WebDashboard::startListening() {
#if defined(ARDUINO) || defined(ESP32)
    if (m_running) {
        return true; // Already active
    }
    if (m_server == nullptr) {
        return false;
    }

    // Verify Wi-Fi is fully connected with a valid IP before calling m_server->begin()
    if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == INADDR_NONE) {
        return false;
    }

    m_server->begin();

    m_running = true;

    // Initialize mDNS responder for http://voltwatch.local
    if (MDNS.begin(Config::MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", m_port);
        m_mdnsActive = true;
        Serial.printf("[WebDashboard] HTTP server listening on port %u\n", m_port);
        Serial.printf("[WebDashboard] mDNS responder active: http://%s.local\n", Config::MDNS_HOSTNAME);
    } else {
        m_mdnsActive = false;
        Serial.println("[WebDashboard] WARNING: Failed to start mDNS responder!");
    }
    return true;
#else
    m_running = true;
    m_mdnsActive = true;
    return true;
#endif
}

void WebDashboard::stop() {
#if defined(ARDUINO) || defined(ESP32)
    if (m_server) {
        m_server->stop();
        delete m_server;
        m_server = nullptr;
    }
#endif
    m_running = false;
    m_mdnsActive = false;
    m_pendingStart = false;
}

void WebDashboard::updateStatus(const PowerSystemStatus& status) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_status = status;
}

void WebDashboard::updateHistory(const OutageRecord* records, size_t count) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (!records || count == 0) {
        m_historyCount = 0;
        return;
    }

    m_historyCount = (count > Config::OUTAGE_HISTORY_MAX) ? Config::OUTAGE_HISTORY_MAX : count;
    for (size_t i = 0; i < m_historyCount; ++i) {
        m_history[i] = records[i];
    }
}

void WebDashboard::update(uint32_t currentMillis) {
    (void)currentMillis;
#if defined(ARDUINO) || defined(ESP32)
    // Synchronously activate web server on the main loop task once Wi-Fi has allocated an IP
    if (!m_running && (m_pendingStart.load() || (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE))) {
        m_pendingStart = false;
        startListening();
    }

    if (m_running && m_server) {
        m_server->handleClient();
    }
#endif
}



// =============================================================================
// JSON Serialization Logic
// =============================================================================

size_t WebDashboard::serializeStatusJson(const PowerSystemStatus& status, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return 0;
    }

    const char* gStatus = gridStatusString(status.state);

    int written = snprintf(
        buffer,
        bufferSize,
        "{\"system\":\"VoltWatch\","
        "\"uptime_seconds\":%u,"
        "\"grid\":{\"status\":\"%s\",\"voltage\":%.1f,\"frequency\":%.1f},"
        "\"inverter\":{\"load_watts\":%.1f,\"current_amps\":%.2f,\"power_factor\":%.2f,\"energy_kwh\":%.2f},"
        "\"battery\":{\"est_runtime_minutes\":%u}}",
        status.uptimeSeconds,
        gStatus,
        status.grid.voltage,
        status.grid.frequency,
        status.inverter.power,
        status.inverter.current,
        status.inverter.powerFactor,
        status.inverter.energy,
        status.estBatteryRuntimeMinutes
    );

    if (written < 0) {
        buffer[0] = '\0';
        return 0;
    }

    if (static_cast<size_t>(written) >= bufferSize) {
        buffer[bufferSize - 1] = '\0';
        return bufferSize - 1;
    }

    return static_cast<size_t>(written);
}

std::string WebDashboard::buildStatusJson(const PowerSystemStatus& status) {
    char buf[512];
    size_t len = serializeStatusJson(status, buf, sizeof(buf));
    return std::string(buf, len);
}

size_t WebDashboard::serializeHistoryJson(const OutageRecord* records, size_t count, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return 0;
    }

    if (!records || count == 0) {
        if (bufferSize < 3) {
            buffer[0] = '\0';
            return 0;
        }
        buffer[0] = '[';
        buffer[1] = ']';
        buffer[2] = '\0';
        return 2;
    }

    size_t limit = (count > Config::OUTAGE_HISTORY_MAX) ? Config::OUTAGE_HISTORY_MAX : count;

    size_t written = 0;
    buffer[written++] = '[';

    for (size_t i = 0; i < limit; ++i) {
        char itemBuf[256];
        int itemLen = snprintf(
            itemBuf,
            sizeof(itemBuf),
            "%s{\"start_timestamp\":%u,\"end_timestamp\":%u,\"duration_seconds\":%u,\"grid_voltage_before_trip\":%.1f,\"initial_load_watts\":%.1f,\"energy_consumed_kwh\":%.3f}",
            (i > 0 ? "," : ""),
            records[i].startTimestamp,
            records[i].endTimestamp,
            records[i].durationSeconds,
            records[i].gridVoltageBeforeTrip,
            records[i].initialLoadWatts,
            records[i].energyConsumedKwh
        );

        if (itemLen < 0) {
            buffer[0] = '\0';
            return 0;
        }

        // Check if item fits including closing bracket and null terminator
        if (written + static_cast<size_t>(itemLen) + 2 > bufferSize) {
            break;
        }

        memcpy(buffer + written, itemBuf, itemLen);
        written += itemLen;
    }

    if (written + 2 <= bufferSize) {
        buffer[written++] = ']';
        buffer[written] = '\0';
    } else {
        buffer[bufferSize - 2] = ']';
        buffer[bufferSize - 1] = '\0';
        written = bufferSize - 1;
    }

    return written;
}

std::string WebDashboard::buildHistoryJson(const OutageRecord* records, size_t count) {
    if (!records || count == 0) {
        return "[]";
    }

    size_t limit = (count > Config::OUTAGE_HISTORY_MAX) ? Config::OUTAGE_HISTORY_MAX : count;
    std::string result = "[";

    for (size_t i = 0; i < limit; ++i) {
        if (i > 0) result += ",";
        char itemBuf[256];
        snprintf(
            itemBuf,
            sizeof(itemBuf),
            "{\"start_timestamp\":%u,\"end_timestamp\":%u,\"duration_seconds\":%u,\"grid_voltage_before_trip\":%.1f,\"initial_load_watts\":%.1f,\"energy_consumed_kwh\":%.3f}",
            records[i].startTimestamp,
            records[i].endTimestamp,
            records[i].durationSeconds,
            records[i].gridVoltageBeforeTrip,
            records[i].initialLoadWatts,
            records[i].energyConsumedKwh
        );
        result += itemBuf;
    }
    result += "]";
    return result;
}

std::string WebDashboard::buildHistoryJson(const std::vector<OutageRecord>& records) {
    return buildHistoryJson(records.data(), records.size());
}

#if !defined(ARDUINO) && !defined(ESP32)
WebDashboard::MockResponse WebDashboard::handleSimulatedRequest(const std::string& method, const std::string& path) {
    MockResponse resp;

    if (method == "OPTIONS") {
        resp.statusCode = 204;
        resp.contentType = "text/plain";
        resp.body = "";
        resp.headers.push_back({"Access-Control-Allow-Origin", "*"});
        resp.headers.push_back({"Access-Control-Allow-Methods", "GET, POST, OPTIONS"});
        resp.headers.push_back({"Access-Control-Allow-Headers", "Content-Type, Access-Control-Request-Private-Network"});
        resp.headers.push_back({"Access-Control-Allow-Private-Network", "true"});
        return resp;
    }

    if (method != "GET") {
        resp.statusCode = 405;
        resp.contentType = "application/json";
        resp.body = "{\"error\":\"Method Not Allowed\"}";
        return resp;
    }

    if (path == "/api/status") {
        resp.statusCode = 200;
        resp.contentType = "application/json";
        resp.body = buildStatusJson(m_status);
    } else if (path == "/api/history") {
        resp.statusCode = 200;
        resp.contentType = "application/json";
        resp.body = buildHistoryJson(m_history, m_historyCount);
    } else if (path == "/" || path == "/index.html") {
        resp.statusCode = 200;
        resp.contentType = "application/json";
        resp.body = "{\"service\":\"VoltWatch\",\"status\":\"online\",\"api\":[\"/api/status\",\"/api/history\",\"/api/events\"]}";
    } else if (path == "/api/events") {
        resp.statusCode = 200;
        resp.contentType = "text/event-stream";
        resp.body = "event: status\ndata: " + buildStatusJson(m_status) + "\n\n";
    } else {
        resp.statusCode = 404;
        resp.contentType = "application/json";
        resp.body = "{\"error\":\"Not Found\"}";
    }

    return resp;
}
#endif

} // namespace VoltWatch
