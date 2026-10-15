#include "WiFiManager.h"

#include <algorithm>

#if defined(ARDUINO) && !defined(UNIT_TEST)
#include <Arduino.h>
#include <WiFi.h>
#endif

namespace VoltWatch {

WiFiManager::WiFiManager() {
#if !defined(ARDUINO) || defined(UNIT_TEST)
    m_mockMode = true;
#endif
}

void WiFiManager::init(const char* ssid, const char* password) {
    m_ssid = (ssid != nullptr) ? ssid : WIFI_SSID;
    m_password = (password != nullptr) ? password : WIFI_PASSWORD;

#if defined(ARDUINO) && !defined(UNIT_TEST)
    if (!m_mockMode) {
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(false); // We control reconnection with exponential backoff
    }
#endif

    startConnecting(0);
}

void WiFiManager::setBackoffParams(uint32_t minBackoffMs, uint32_t maxBackoffMs, uint32_t timeoutMs) {
    m_minBackoffMs = minBackoffMs;
    m_maxBackoffMs = maxBackoffMs;
    m_connectTimeoutMs = timeoutMs;
    m_currentBackoffMs = minBackoffMs;
}

void WiFiManager::startConnecting(uint32_t currentMillis) {
    m_state = WiFiState::CONNECTING;
    m_lastStateChangeMs = currentMillis;

#if defined(ARDUINO) && !defined(UNIT_TEST)
    if (!m_mockMode) {
        Serial.printf("[WiFi] Connecting to SSID: '%s'...\n", m_ssid.c_str());
        WiFi.begin(m_ssid.c_str(), m_password.c_str());
    }
#endif
}

void WiFiManager::update(uint32_t currentMillis) {
    switch (m_state) {
        case WiFiState::DISCONNECTED:
            // Idle state, no active connection or retry scheduled
            break;

        case WiFiState::CONNECTING: {
#if defined(ARDUINO) && !defined(UNIT_TEST)
            if (!m_mockMode) {
                if (WiFi.status() == WL_CONNECTED) {
                    m_state = WiFiState::CONNECTED;
                    m_currentBackoffMs = m_minBackoffMs;
                    m_reconnectAttempts = 0;
                    m_successfulConnections++;
                    m_ipAddress = WiFi.localIP().toString().c_str();
                    m_rssi = WiFi.RSSI();
                    m_lastStateChangeMs = currentMillis;
                    m_lastRssiCheckMs = currentMillis;
                    Serial.printf("[WiFi] Connected! IP: %s | RSSI: %d dBm\n", m_ipAddress.c_str(), m_rssi);
                } else if (currentMillis - m_lastStateChangeMs >= m_connectTimeoutMs) {
                    Serial.println("[WiFi] Connection attempt timed out. Waiting before retry...");
                    WiFi.disconnect(true);
                    m_reconnectAttempts++;
                    m_currentBackoffMs = std::min(m_currentBackoffMs * 2, m_maxBackoffMs);
                    m_state = WiFiState::WAITING_RECONNECT;
                    m_lastStateChangeMs = currentMillis;
                }
                break;
            }
#endif
            if (m_mockConnectedTarget) {
                m_state = WiFiState::CONNECTED;
                m_currentBackoffMs = m_minBackoffMs;
                m_reconnectAttempts = 0;
                m_successfulConnections++;
                m_ipAddress = "192.168.1.150";
                m_rssi = -62;
                m_lastStateChangeMs = currentMillis;
                m_lastRssiCheckMs = currentMillis;
            } else if (currentMillis - m_lastStateChangeMs >= m_connectTimeoutMs) {
                m_reconnectAttempts++;
                m_currentBackoffMs = std::min(m_currentBackoffMs * 2, m_maxBackoffMs);
                m_state = WiFiState::WAITING_RECONNECT;
                m_lastStateChangeMs = currentMillis;
            }
            break;
        }

        case WiFiState::CONNECTED: {
#if defined(ARDUINO) && !defined(UNIT_TEST)
            if (!m_mockMode) {
                if (WiFi.status() != WL_CONNECTED) {
                    Serial.println("[WiFi] Lost connection. Reconnecting with exponential backoff...");
                    WiFi.disconnect(true);
                    m_reconnectAttempts++;
                    m_currentBackoffMs = m_minBackoffMs;
                    m_state = WiFiState::WAITING_RECONNECT;
                    m_ipAddress = "0.0.0.0";
                    m_rssi = 0;
                    m_lastStateChangeMs = currentMillis;
                } else if (currentMillis - m_lastRssiCheckMs >= 5000) {
                    m_lastRssiCheckMs = currentMillis;
                    m_rssi = WiFi.RSSI();
                }
                break;
            }
#endif
            if (!m_mockConnectedTarget) {
                m_reconnectAttempts++;
                m_currentBackoffMs = m_minBackoffMs;
                m_state = WiFiState::WAITING_RECONNECT;
                m_ipAddress = "0.0.0.0";
                m_rssi = 0;
                m_lastStateChangeMs = currentMillis;
            }
            break;
        }

        case WiFiState::WAITING_RECONNECT: {
            if (currentMillis - m_lastStateChangeMs >= m_currentBackoffMs) {
                startConnecting(currentMillis);
            }
            break;
        }
    }
}

void WiFiManager::forceReconnect(uint32_t currentMillis) {
#if defined(ARDUINO) && !defined(UNIT_TEST)
    if (!m_mockMode) {
        WiFi.disconnect(true);
    }
#endif
    startConnecting(currentMillis);
}

void WiFiManager::disconnect() {
#if defined(ARDUINO) && !defined(UNIT_TEST)
    if (!m_mockMode) {
        WiFi.disconnect(true);
    }
#endif
    m_state = WiFiState::DISCONNECTED;
    m_ipAddress = "0.0.0.0";
    m_rssi = 0;
}

void WiFiManager::setMockConnected(bool connected) {
    m_mockConnectedTarget = connected;
}

void WiFiManager::simulateDisconnect() {
    m_mockConnectedTarget = false;
}

} // namespace VoltWatch
