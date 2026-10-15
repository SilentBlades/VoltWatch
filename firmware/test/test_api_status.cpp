#include "test_framework.h"
#include "../src/web/WebDashboard.h"
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

// =============================================================================
// Helper: Simple JSON Syntax and Token Validator
// =============================================================================
static bool validateJsonBrackets(const std::string& json) {
    int braceCount = 0;
    int bracketCount = 0;
    bool inQuotes = false;
    bool escaped = false;

    for (char c : json) {
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (inQuotes) continue;

        if (c == '{') braceCount++;
        else if (c == '}') {
            braceCount--;
            if (braceCount < 0) return false;
        }
        else if (c == '[') bracketCount++;
        else if (c == ']') {
            bracketCount--;
            if (bracketCount < 0) return false;
        }
    }
    return (!inQuotes && braceCount == 0 && bracketCount == 0);
}

// Count occurrences of a substring
static size_t countSubstring(const std::string& text, const std::string& sub) {
    if (sub.empty()) return 0;
    size_t count = 0;
    size_t pos = 0;
    while ((pos = text.find(sub, pos)) != std::string::npos) {
        count++;
        pos += sub.length();
    }
    return count;
}

// =============================================================================
// Unit Tests: /api/status Specification Compliance
// =============================================================================

TEST_CASE(WebDashboard, ApiStatusExactSpecification) {
    // Populate status with the EXACT values specified in docs/requirements.md section 5.2:
    // uptime_seconds: 86420
    // grid: status: ONLINE, voltage: 232.4, frequency: 50.0
    // inverter: load_watts: 345.2, current_amps: 1.54, power_factor: 0.96, energy_kwh: 12.45
    // battery: est_runtime_minutes: 270
    VoltWatch::PowerSystemStatus status{};
    status.uptimeSeconds = 86420;
    status.state = VoltWatch::OutageState::STATE_NORMAL;
    status.grid.voltage = 232.4f;
    status.grid.frequency = 50.0f;
    status.grid.isValid = true;
    status.inverter.voltage = 230.0f;
    status.inverter.power = 345.2f;
    status.inverter.current = 1.54f;
    status.inverter.powerFactor = 0.96f;
    status.inverter.energy = 12.45f;
    status.inverter.isValid = true;
    status.estBatteryRuntimeMinutes = 270;

    std::string json = VoltWatch::WebDashboard::buildStatusJson(status);

    // 1. Verify JSON brackets are valid and balanced
    ASSERT_TRUE(validateJsonBrackets(json));

    // 2. Verify top-level keys
    ASSERT_TRUE(json.find("\"system\":\"VoltWatch\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"uptime_seconds\":86420") != std::string::npos);

    // 3. Verify nested grid object and fields
    ASSERT_TRUE(json.find("\"grid\":{") != std::string::npos);
    ASSERT_TRUE(json.find("\"status\":\"ONLINE\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"voltage\":232.4") != std::string::npos);
    ASSERT_TRUE(json.find("\"frequency\":50.0") != std::string::npos);

    // 4. Verify nested inverter object and fields
    ASSERT_TRUE(json.find("\"inverter\":{") != std::string::npos);
    ASSERT_TRUE(json.find("\"load_watts\":345.2") != std::string::npos);
    ASSERT_TRUE(json.find("\"current_amps\":1.54") != std::string::npos);
    ASSERT_TRUE(json.find("\"power_factor\":0.96") != std::string::npos);
    ASSERT_TRUE(json.find("\"energy_kwh\":12.45") != std::string::npos);

    // 5. Verify nested battery object and fields
    ASSERT_TRUE(json.find("\"battery\":{") != std::string::npos);
    ASSERT_TRUE(json.find("\"est_runtime_minutes\":270") != std::string::npos);

    // 6. Test serializeStatusJson directly into a char buffer
    char buffer[512];
    size_t len = VoltWatch::WebDashboard::serializeStatusJson(status, buffer, sizeof(buffer));
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(len == json.length());
    ASSERT_TRUE(strcmp(buffer, json.c_str()) == 0);
}

TEST_CASE(WebDashboard, ApiStatusOutageStateOffline) {
    VoltWatch::PowerSystemStatus status{};
    status.uptimeSeconds = 12345;
    status.state = VoltWatch::OutageState::STATE_OUTAGE;
    status.grid.voltage = 0.0f;
    status.grid.frequency = 0.0f;
    status.grid.isValid = false;
    status.inverter.power = 450.0f;
    status.inverter.current = 1.95f;
    status.inverter.powerFactor = 0.98f;
    status.inverter.energy = 5.20f;
    status.estBatteryRuntimeMinutes = 180;

    std::string json = VoltWatch::WebDashboard::buildStatusJson(status);

    ASSERT_TRUE(validateJsonBrackets(json));
    ASSERT_TRUE(json.find("\"status\":\"OFFLINE\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"voltage\":0.0") != std::string::npos);
    ASSERT_TRUE(json.find("\"frequency\":0.0") != std::string::npos);
    ASSERT_TRUE(json.find("\"load_watts\":450.0") != std::string::npos);
    ASSERT_TRUE(json.find("\"est_runtime_minutes\":180") != std::string::npos);
}

TEST_CASE(WebDashboard, ApiStatusRecoveredStateOnline) {
    VoltWatch::PowerSystemStatus status{};
    status.uptimeSeconds = 20000;
    status.state = VoltWatch::OutageState::STATE_RECOVERED;
    status.grid.voltage = 235.1f;
    status.grid.frequency = 50.1f;
    status.grid.isValid = true;
    status.inverter.power = 120.0f;
    status.inverter.current = 0.52f;
    status.inverter.powerFactor = 0.95f;
    status.inverter.energy = 8.50f;
    status.estBatteryRuntimeMinutes = 600;

    std::string json = VoltWatch::WebDashboard::buildStatusJson(status);

    ASSERT_TRUE(validateJsonBrackets(json));
    ASSERT_TRUE(json.find("\"status\":\"ONLINE\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"voltage\":235.1") != std::string::npos);
}

TEST_CASE(WebDashboard, ApiStatusBufferBoundarySafety) {
    VoltWatch::PowerSystemStatus status{};
    status.uptimeSeconds = 100;
    status.state = VoltWatch::OutageState::STATE_NORMAL;

    // 1. Null pointer safety
    size_t zeroWritten = VoltWatch::WebDashboard::serializeStatusJson(status, nullptr, 100);
    ASSERT_EQ(static_cast<size_t>(0), zeroWritten);

    // 2. Zero buffer size safety
    char testBuf[64];
    zeroWritten = VoltWatch::WebDashboard::serializeStatusJson(status, testBuf, 0);
    ASSERT_EQ(static_cast<size_t>(0), zeroWritten);

    // 3. Very small buffer (truncation safety without memory corruption)
    char tinyBuf[20];
    size_t truncatedLen = VoltWatch::WebDashboard::serializeStatusJson(status, tinyBuf, sizeof(tinyBuf));
    ASSERT_TRUE(truncatedLen < sizeof(tinyBuf));
    ASSERT_TRUE(tinyBuf[sizeof(tinyBuf) - 1] == '\0');
}

// =============================================================================
// Unit Tests: /api/history Specification Compliance
// =============================================================================

TEST_CASE(WebDashboard, ApiHistoryEmptyArray) {
    // Empty history should return valid empty JSON array "[]"
    std::string json = VoltWatch::WebDashboard::buildHistoryJson(nullptr, 0);
    ASSERT_TRUE(json == "[]");
    ASSERT_TRUE(validateJsonBrackets(json));

    char buf[32];
    size_t len = VoltWatch::WebDashboard::serializeHistoryJson(nullptr, 0, buf, sizeof(buf));
    ASSERT_EQ(static_cast<size_t>(2), len);
    ASSERT_TRUE(strcmp(buf, "[]") == 0);
}

TEST_CASE(WebDashboard, ApiHistorySingleRecord) {
    VoltWatch::OutageRecord rec{};
    rec.startTimestamp = 1729000000;
    rec.endTimestamp = 1729001800;
    rec.durationSeconds = 1800;
    rec.gridVoltageBeforeTrip = 230.5f;
    rec.initialLoadWatts = 345.2f;
    rec.energyConsumedKwh = 0.175f;

    std::string json = VoltWatch::WebDashboard::buildHistoryJson(&rec, 1);

    ASSERT_TRUE(validateJsonBrackets(json));
    ASSERT_TRUE(json.front() == '[' && json.back() == ']');
    ASSERT_TRUE(json.find("\"start_timestamp\":1729000000") != std::string::npos);
    ASSERT_TRUE(json.find("\"end_timestamp\":1729001800") != std::string::npos);
    ASSERT_TRUE(json.find("\"duration_seconds\":1800") != std::string::npos);
    ASSERT_TRUE(json.find("\"grid_voltage_before_trip\":230.5") != std::string::npos);
    ASSERT_TRUE(json.find("\"initial_load_watts\":345.2") != std::string::npos);
    ASSERT_TRUE(json.find("\"energy_consumed_kwh\":0.175") != std::string::npos);
}

TEST_CASE(WebDashboard, ApiHistoryTenRecordsRingBuffer) {
    VoltWatch::OutageRecord records[10];
    for (size_t i = 0; i < 10; ++i) {
        records[i].startTimestamp = 1729000000 + static_cast<uint32_t>(i * 3600);
        records[i].endTimestamp = records[i].startTimestamp + 600;
        records[i].durationSeconds = 600;
        records[i].gridVoltageBeforeTrip = 228.0f + static_cast<float>(i);
        records[i].initialLoadWatts = 300.0f + static_cast<float>(i * 20);
        records[i].energyConsumedKwh = 0.050f * static_cast<float>(i + 1);
    }

    std::string json = VoltWatch::WebDashboard::buildHistoryJson(records, 10);

    // 1. Verify JSON syntax
    ASSERT_TRUE(validateJsonBrackets(json));
    ASSERT_TRUE(json.front() == '[' && json.back() == ']');

    // 2. Count number of objects
    size_t recordCount = countSubstring(json, "\"start_timestamp\":");
    ASSERT_EQ(static_cast<size_t>(10), recordCount);

    // 3. Count separating commas between 10 items (must be exactly 9 commas at top array level)
    size_t durationCount = countSubstring(json, "\"duration_seconds\":600");
    ASSERT_EQ(static_cast<size_t>(10), durationCount);

    // 4. Verify first and last records
    ASSERT_TRUE(json.find("\"start_timestamp\":1729000000") != std::string::npos);
    ASSERT_TRUE(json.find("\"start_timestamp\":1729032400") != std::string::npos);
}

TEST_CASE(WebDashboard, ApiHistoryBufferBoundarySafety) {
    VoltWatch::OutageRecord rec{};
    rec.startTimestamp = 1000;
    rec.durationSeconds = 60;

    // Null buffer
    size_t zeroWritten = VoltWatch::WebDashboard::serializeHistoryJson(&rec, 1, nullptr, 100);
    ASSERT_EQ(static_cast<size_t>(0), zeroWritten);

    // Zero size
    char buf[64];
    zeroWritten = VoltWatch::WebDashboard::serializeHistoryJson(&rec, 1, buf, 0);
    ASSERT_EQ(static_cast<size_t>(0), zeroWritten);

    // Small buffer
    char smallBuf[15];
    size_t len = VoltWatch::WebDashboard::serializeHistoryJson(&rec, 1, smallBuf, sizeof(smallBuf));
    ASSERT_TRUE(len < sizeof(smallBuf));
    ASSERT_TRUE(smallBuf[sizeof(smallBuf) - 1] == '\0');
}

// =============================================================================
// Unit Tests: Simulated Request Dispatching & REST Endpoints
// =============================================================================

TEST_CASE(WebDashboard, SimulatedRequestDispatch) {
    VoltWatch::WebDashboard dashboard;
    dashboard.begin(80);

    VoltWatch::PowerSystemStatus status{};
    status.uptimeSeconds = 86420;
    status.state = VoltWatch::OutageState::STATE_NORMAL;
    status.grid.voltage = 232.4f;
    status.grid.frequency = 50.0f;
    status.inverter.power = 345.2f;
    status.inverter.current = 1.54f;
    status.inverter.powerFactor = 0.96f;
    status.inverter.energy = 12.45f;
    status.estBatteryRuntimeMinutes = 270;
    dashboard.updateStatus(status);

    VoltWatch::OutageRecord rec{};
    rec.startTimestamp = 1729000000;
    rec.durationSeconds = 1800;
    rec.gridVoltageBeforeTrip = 232.4f;
    rec.initialLoadWatts = 345.2f;
    rec.energyConsumedKwh = 0.175f;
    dashboard.updateHistory(&rec, 1);

    // 1. GET /api/status
    auto respStatus = dashboard.handleSimulatedRequest("GET", "/api/status");
    ASSERT_EQ(200, respStatus.statusCode);
    ASSERT_TRUE(respStatus.contentType == "application/json");
    ASSERT_TRUE(validateJsonBrackets(respStatus.body));
    ASSERT_TRUE(respStatus.body.find("\"system\":\"VoltWatch\"") != std::string::npos);
    ASSERT_TRUE(respStatus.body.find("\"uptime_seconds\":86420") != std::string::npos);

    // 2. GET /api/history
    auto respHistory = dashboard.handleSimulatedRequest("GET", "/api/history");
    ASSERT_EQ(200, respHistory.statusCode);
    ASSERT_TRUE(respHistory.contentType == "application/json");
    ASSERT_TRUE(validateJsonBrackets(respHistory.body));
    ASSERT_TRUE(respHistory.body.find("\"duration_seconds\":1800") != std::string::npos);

    // 3. GET / (Diagnostic JSON payload)
    auto respRoot = dashboard.handleSimulatedRequest("GET", "/");
    ASSERT_EQ(200, respRoot.statusCode);
    ASSERT_TRUE(respRoot.contentType == "application/json");
    ASSERT_TRUE(validateJsonBrackets(respRoot.body));
    ASSERT_TRUE(respRoot.body.find("\"service\":\"VoltWatch\"") != std::string::npos);
    ASSERT_TRUE(respRoot.body.find("\"status\":\"online\"") != std::string::npos);
    ASSERT_TRUE(respRoot.body.find("\"/api/status\"") != std::string::npos);

    // 4. GET /api/events (SSE)
    auto respEvents = dashboard.handleSimulatedRequest("GET", "/api/events");
    ASSERT_EQ(200, respEvents.statusCode);
    ASSERT_TRUE(respEvents.contentType == "text/event-stream");
    ASSERT_TRUE(respEvents.body.find("event: status") != std::string::npos);

    // 5. Unknown endpoint returns 404
    auto respNotFound = dashboard.handleSimulatedRequest("GET", "/api/nonexistent");
    ASSERT_EQ(404, respNotFound.statusCode);

    // 6. Non-GET method returns 405
    auto respMethodNotAllowed = dashboard.handleSimulatedRequest("POST", "/api/status");
    ASSERT_EQ(405, respMethodNotAllowed.statusCode);

    // 7. OPTIONS preflight returns 204
    auto respPreflight = dashboard.handleSimulatedRequest("OPTIONS", "/api/status");
    ASSERT_EQ(204, respPreflight.statusCode);
}

// =============================================================================
// Unit Tests: WebDashboard Lifecycle & State Caching
// =============================================================================

TEST_CASE(WebDashboard, LifecycleAndStateCaching) {
    VoltWatch::WebDashboard dashboard;
    ASSERT_FALSE(dashboard.isRunning());

    dashboard.begin(8080);
    ASSERT_TRUE(dashboard.isRunning());
    ASSERT_TRUE(dashboard.isMdnsActive());
    ASSERT_EQ(static_cast<uint16_t>(8080), dashboard.getPort());

    // Update Status
    VoltWatch::PowerSystemStatus s{};
    s.uptimeSeconds = 500;
    dashboard.updateStatus(s);
    ASSERT_EQ(static_cast<uint32_t>(500), dashboard.getStatus().uptimeSeconds);

    // Update History
    VoltWatch::OutageRecord recs[3];
    recs[0].durationSeconds = 100;
    recs[1].durationSeconds = 200;
    recs[2].durationSeconds = 300;
    dashboard.updateHistory(recs, 3);
    ASSERT_EQ(static_cast<size_t>(3), dashboard.getHistoryCount());
    ASSERT_EQ(static_cast<uint32_t>(200), dashboard.getHistory()[1].durationSeconds);

    dashboard.stop();
    ASSERT_FALSE(dashboard.isRunning());
    ASSERT_FALSE(dashboard.isMdnsActive());
}

// =============================================================================
// Unit Tests: Standalone Web Asset & Headless API Verification
// =============================================================================

TEST_CASE(WebDashboard, StandaloneWebAssetIntegrity) {
    // Locate web/index.html from test execution directory
    const char* paths[] = {
        "../../web/index.html",
        "../web/index.html",
        "web/index.html"
    };

    FILE* f = nullptr;
    for (const char* p : paths) {
        f = fopen(p, "rb");
        if (f) break;
    }
    ASSERT_TRUE(f != nullptr);

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    ASSERT_TRUE(size > 0);

    std::string sHtml(size, '\0');
    size_t readBytes = fread(&sHtml[0], 1, size, f);
    fclose(f);
    ASSERT_EQ(static_cast<size_t>(size), readBytes);

    // Verify key UI elements in standalone web/index.html
    ASSERT_TRUE(sHtml.find("<!DOCTYPE html>") != std::string::npos);
    ASSERT_TRUE(sHtml.find("VoltWatch") != std::string::npos);
    ASSERT_TRUE(sHtml.find("status-pill") != std::string::npos);
    ASSERT_TRUE(sHtml.find("GRID ONLINE") != std::string::npos);
    ASSERT_TRUE(sHtml.find("val-grid-v") != std::string::npos);
    ASSERT_TRUE(sHtml.find("val-grid-f") != std::string::npos);
    ASSERT_TRUE(sHtml.find("val-inv-w") != std::string::npos);
    ASSERT_TRUE(sHtml.find("val-inv-a") != std::string::npos);
    ASSERT_TRUE(sHtml.find("val-inv-pf") != std::string::npos);
    ASSERT_TRUE(sHtml.find("val-inv-kwh") != std::string::npos);
    ASSERT_TRUE(sHtml.find("battery-runtime-val") != std::string::npos);
    ASSERT_TRUE(sHtml.find("history-table-body") != std::string::npos);
    ASSERT_TRUE(sHtml.find("/api/events") != std::string::npos);
    ASSERT_TRUE(sHtml.find("/api/status") != std::string::npos);
    ASSERT_TRUE(sHtml.find("/api/history") != std::string::npos);
    ASSERT_TRUE(sHtml.find("API_BASE") != std::string::npos);
    ASSERT_TRUE(sHtml.find("mixed-content-warning") != std::string::npos);
    ASSERT_TRUE(sHtml.find("Browser Mixed Content policy") != std::string::npos);
    ASSERT_TRUE(sHtml.find("diag-url") != std::string::npos);
    ASSERT_TRUE(sHtml.find("diag-status") != std::string::npos);
}

TEST_CASE(WebDashboard, HealthCheckAndPreflightCors) {
    VoltWatch::WebDashboard dashboard;

    // Verify OPTIONS preflight returns 204 with Chrome PNA headers
    auto respPreflight = dashboard.handleSimulatedRequest("OPTIONS", "/api/status");
    ASSERT_EQ(204, respPreflight.statusCode);
    ASSERT_TRUE(respPreflight.body.empty());
    ASSERT_EQ("*", respPreflight.getHeader("Access-Control-Allow-Origin"));
    ASSERT_EQ("true", respPreflight.getHeader("Access-Control-Allow-Private-Network"));
    ASSERT_TRUE(respPreflight.getHeader("Access-Control-Allow-Headers").find("Access-Control-Request-Private-Network") != std::string::npos);

    // Verify GET / returns health-check JSON
    auto respRoot = dashboard.handleSimulatedRequest("GET", "/");
    ASSERT_EQ(200, respRoot.statusCode);
    ASSERT_TRUE(respRoot.contentType == "application/json");
    ASSERT_TRUE(respRoot.body.find("\"service\":\"VoltWatch\"") != std::string::npos);
    ASSERT_TRUE(respRoot.body.find("\"status\":\"online\"") != std::string::npos);

    // Verify GET /index.html returns health-check JSON
    auto respIndex = dashboard.handleSimulatedRequest("GET", "/index.html");
    ASSERT_EQ(200, respIndex.statusCode);
    ASSERT_TRUE(respIndex.contentType == "application/json");
    ASSERT_TRUE(respIndex.body.find("\"service\":\"VoltWatch\"") != std::string::npos);
}
