#ifndef VOLTWATCH_WIFI_MANAGER_H
#define VOLTWATCH_WIFI_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <ostream>

#include "include/config.h"

namespace VoltWatch {

/**
 * @brief Wi-Fi connection states
 */
enum class WiFiState : uint8_t {
    DISCONNECTED = 0,     ///< Wi-Fi interface down or idle
    CONNECTING,           ///< Connection attempt in progress (non-blocking)
    CONNECTED,            ///< Successfully connected with valid IP address
    WAITING_RECONNECT     ///< Waiting for exponential backoff interval before retrying
};

inline const char* wifiStateToString(WiFiState state) {
    switch (state) {
        case WiFiState::DISCONNECTED:        return "DISCONNECTED";
        case WiFiState::CONNECTING:          return "CONNECTING";
        case WiFiState::CONNECTED:           return "CONNECTED";
        case WiFiState::WAITING_RECONNECT:   return "WAITING_RECONNECT";
        default:                             return "UNKNOWN";
    }
}

inline std::ostream& operator<<(std::ostream& os, WiFiState state) {
    return os << wifiStateToString(state);
}

/**
 * @brief Non-blocking asynchronous Wi-Fi manager with exponential backoff auto-reconnect.
 *
 * Guarantees that PZEM-004T metrology polling is NEVER blocked during Wi-Fi drops,
 * signal fading, or AP router reboots.
 */
class WiFiManager {
public:
    static constexpr uint32_t DEFAULT_MIN_BACKOFF_MS = 2000;    // 2 seconds initial backoff
    static constexpr uint32_t DEFAULT_MAX_BACKOFF_MS = 60000;   // 60 seconds maximum backoff
    static constexpr uint32_t DEFAULT_CONNECT_TIMEOUT_MS = 10000; // 10 seconds attempt timeout

    WiFiManager();
    ~WiFiManager() = default;

    /**
     * @brief Configure credentials and start non-blocking connection
     */
    void init(const char* ssid = WIFI_SSID,
              const char* password = WIFI_PASSWORD);

    /**
     * @brief Non-blocking state machine tick. Call regularly from loop().
     * @param currentMillis Current microcontroller uptime in milliseconds
     */
    void update(uint32_t currentMillis);

    // --- State and Metric Queries ---
    bool isConnected() const { return m_state == WiFiState::CONNECTED; }
    WiFiState getState() const { return m_state; }
    const char* getStateString() const { return wifiStateToString(m_state); }
    const char* getIpAddress() const { return m_ipAddress.c_str(); }
    int8_t getRssi() const { return m_rssi; }

    uint32_t getCurrentBackoffIntervalMs() const { return m_currentBackoffMs; }
    uint32_t getReconnectAttempts() const { return m_reconnectAttempts; }
    uint32_t getSuccessfulConnections() const { return m_successfulConnections; }

    // --- Control Commands ---
    void forceReconnect(uint32_t currentMillis);
    void disconnect();
    void setBackoffParams(uint32_t minBackoffMs, uint32_t maxBackoffMs, uint32_t timeoutMs);

    // --- Native Mocking & Simulation ---
    void setMockMode(bool enabled) { m_mockMode = enabled; }
    bool isMockMode() const { return m_mockMode; }
    void setMockConnected(bool connected);
    void simulateDisconnect();

private:
    void startConnecting(uint32_t currentMillis);

    std::string m_ssid;
    std::string m_password;
    std::string m_ipAddress{"0.0.0.0"};
    int8_t m_rssi{0};

    WiFiState m_state{WiFiState::DISCONNECTED};
    uint32_t m_minBackoffMs{DEFAULT_MIN_BACKOFF_MS};
    uint32_t m_maxBackoffMs{DEFAULT_MAX_BACKOFF_MS};
    uint32_t m_connectTimeoutMs{DEFAULT_CONNECT_TIMEOUT_MS};
    uint32_t m_currentBackoffMs{DEFAULT_MIN_BACKOFF_MS};

    uint32_t m_lastStateChangeMs{0};
    uint32_t m_lastRssiCheckMs{0};
    uint32_t m_reconnectAttempts{0};
    uint32_t m_successfulConnections{0};

    bool m_mockMode{false};
    bool m_mockConnectedTarget{false};
};

} // namespace VoltWatch

#endif // VOLTWATCH_WIFI_MANAGER_H
