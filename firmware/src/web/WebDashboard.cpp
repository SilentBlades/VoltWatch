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
    m_lastSsePushTime = 0;
    m_running = false;
    m_mdnsActive = false;
    m_port = 80;
}

WebDashboard::~WebDashboard() {
    stop();
}

bool WebDashboard::begin(uint16_t port) {
    m_port = port;

#if defined(ARDUINO) || defined(ESP32)
    stop(); // Ensure any prior server instance is torn down

    // Global CORS headers for Headless Metrology / SPA model & Chrome PNA
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Access-Control-Request-Private-Network");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Private-Network", "true");

    m_server = new AsyncWebServer(m_port);
    m_events = new AsyncEventSource("/api/events");

    // Server-Sent Events setup
    m_events->onConnect([](AsyncEventSourceClient *client) {
        if (client->lastId()) {
            Serial.printf("[WebDashboard] SSE client reconnected. Last ID: %u\n", client->lastId());
        }
        client->send("{\"type\":\"connected\"}", "connect", millis(), 10000);
    });
    m_server->addHandler(m_events);

    // Route: Diagnostic JSON payload for "/" and "/index.html"
    auto sendDiagnosticResponse = [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"service\":\"VoltWatch\",\"status\":\"online\",\"api\":[\"/api/status\",\"/api/history\",\"/api/events\"]}");
    };

    m_server->on("/", HTTP_GET, sendDiagnosticResponse);
    m_server->on("/index.html", HTTP_GET, sendDiagnosticResponse);

    // Route: Direct REST JSON Endpoint GET /api/status
    m_server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        std::string json = buildStatusJson(m_status);
        request->send(200, "application/json", json.c_str());
    });

    // Route: REST JSON Endpoint GET /api/history
    m_server->on("/api/history", HTTP_GET, [this](AsyncWebServerRequest *request) {
        std::string json = buildHistoryJson(m_history, m_historyCount);
        request->send(200, "application/json", json.c_str());
    });

    // Handle HTTP OPTIONS preflight requests globally and catch-all 404
    m_server->onNotFound([](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_OPTIONS) {
            AsyncWebServerResponse *response = request->beginResponse(204);
            response->addHeader("Access-Control-Allow-Origin", "*");
            response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            response->addHeader("Access-Control-Allow-Headers", "Content-Type, Access-Control-Request-Private-Network");
            response->addHeader("Access-Control-Allow-Private-Network", "true");
            request->send(response);
        } else {
            request->send(404);
        }
    });

    // Defer socket creation until Wi-Fi allocates an IP to avoid tcp_alloc assertion in ESP32 Core 3.x
#if defined(ARDUINO_EVENT_WIFI_STA_GOT_IP)
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        (void)event;
        (void)info;
        Serial.printf("[WebDashboard] Wi-Fi STA Got IP: %s. Activating WebServer...\n",
                      WiFi.localIP().toString().c_str());
        this->startListening();
    }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
#elif defined(SYSTEM_EVENT_STA_GOT_IP)
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        (void)event;
        (void)info;
        this->startListening();
    }, SYSTEM_EVENT_STA_GOT_IP);
#endif

    // If WiFi happens to already be connected, start listening immediately
    if (WiFi.status() == WL_CONNECTED) {
        startListening();
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
        m_server->end();
        delete m_server;
        m_server = nullptr;
    }
    if (m_events) {
        delete m_events;
        m_events = nullptr;
    }
#endif
    m_running = false;
    m_mdnsActive = false;
}

void WebDashboard::updateStatus(const PowerSystemStatus& status) {
    m_status = status;
}

void WebDashboard::updateHistory(const OutageRecord* records, size_t count) {
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
#if defined(ARDUINO) || defined(ESP32)
    if (!m_running && WiFi.status() == WL_CONNECTED) {
        startListening();
    }
#endif

    if (!m_running) return;

    if (currentMillis - m_lastSsePushTime >= Config::SSE_PUSH_INTERVAL_MS) {
        m_lastSsePushTime = currentMillis;
        pushSseUpdate();
    }
}

void WebDashboard::pushSseUpdate() {
#if defined(ARDUINO) || defined(ESP32)
    if (m_events && m_events->count() > 0) {
        std::string json = buildStatusJson(m_status);
        m_events->send(json.c_str(), "status", millis());
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
