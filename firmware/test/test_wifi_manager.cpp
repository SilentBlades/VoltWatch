#include "test_framework.h"
#include "../src/network/WiFiManager.h"
#include <string>

TEST_CASE(WiFiManager, InitialState) {
    VoltWatch::WiFiManager wifi;
    ASSERT_FALSE(wifi.isConnected());
    ASSERT_EQ(VoltWatch::WiFiState::DISCONNECTED, wifi.getState());
    ASSERT_EQ(std::string("0.0.0.0"), std::string(wifi.getIpAddress()));
    ASSERT_EQ(uint32_t(0), wifi.getReconnectAttempts());
}

TEST_CASE(WiFiManager, NonBlockingConnectSuccess) {
    VoltWatch::WiFiManager wifi;
    wifi.setMockMode(true);
    wifi.setMockConnected(true);

    wifi.init("HomeNetwork", "SecretPass");
    ASSERT_EQ(VoltWatch::WiFiState::CONNECTING, wifi.getState());

    // Update at t = 50ms
    wifi.update(50);

    ASSERT_TRUE(wifi.isConnected());
    ASSERT_EQ(VoltWatch::WiFiState::CONNECTED, wifi.getState());
    ASSERT_EQ(std::string("192.168.1.150"), std::string(wifi.getIpAddress()));
    ASSERT_EQ(uint32_t(2000), wifi.getCurrentBackoffIntervalMs());
    ASSERT_EQ(uint32_t(0), wifi.getReconnectAttempts());
    ASSERT_EQ(uint32_t(1), wifi.getSuccessfulConnections());
}

TEST_CASE(WiFiManager, ExponentialBackoffSequence) {
    VoltWatch::WiFiManager wifi;
    wifi.setMockMode(true);
    wifi.setMockConnected(false); // Fail connection
    wifi.setBackoffParams(2000, 16000, 5000); // min 2s, max 16s, timeout 5s

    wifi.init("FlakyNetwork", "Password");
    ASSERT_EQ(VoltWatch::WiFiState::CONNECTING, wifi.getState());

    // t = 4999ms: Still within 5000ms timeout
    wifi.update(4999);
    ASSERT_EQ(VoltWatch::WiFiState::CONNECTING, wifi.getState());

    // t = 5000ms: Connect attempt times out
    wifi.update(5000);
    ASSERT_EQ(VoltWatch::WiFiState::WAITING_RECONNECT, wifi.getState());
    ASSERT_EQ(uint32_t(1), wifi.getReconnectAttempts());
    // Backoff doubled from 2000 to 4000ms
    ASSERT_EQ(uint32_t(4000), wifi.getCurrentBackoffIntervalMs());

    // t = 8999ms: (5000 + 3999) Still waiting backoff
    wifi.update(8999);
    ASSERT_EQ(VoltWatch::WiFiState::WAITING_RECONNECT, wifi.getState());

    // t = 9000ms: Backoff expired (5000 + 4000), retry initiated!
    wifi.update(9000);
    ASSERT_EQ(VoltWatch::WiFiState::CONNECTING, wifi.getState());

    // t = 14000ms (9000 + 5000): 2nd attempt times out
    wifi.update(14000);
    ASSERT_EQ(VoltWatch::WiFiState::WAITING_RECONNECT, wifi.getState());
    ASSERT_EQ(uint32_t(2), wifi.getReconnectAttempts());
    // Backoff doubled from 4000 to 8000ms
    ASSERT_EQ(uint32_t(8000), wifi.getCurrentBackoffIntervalMs());

    // t = 22000ms (14000 + 8000): 3rd attempt initiated
    wifi.update(22000);
    ASSERT_EQ(VoltWatch::WiFiState::CONNECTING, wifi.getState());

    // t = 27000ms: 3rd attempt times out
    wifi.update(27000);
    ASSERT_EQ(VoltWatch::WiFiState::WAITING_RECONNECT, wifi.getState());
    ASSERT_EQ(uint32_t(3), wifi.getReconnectAttempts());
    // Backoff doubled from 8000 to 16000ms (capped at max 16000)
    ASSERT_EQ(uint32_t(16000), wifi.getCurrentBackoffIntervalMs());

    // t = 43000ms (27000 + 16000): 4th attempt initiated
    wifi.update(43000);
    ASSERT_EQ(VoltWatch::WiFiState::CONNECTING, wifi.getState());

    // Now AP comes back online!
    wifi.setMockConnected(true);
    wifi.update(43100);

    // Reconnected successfully!
    ASSERT_TRUE(wifi.isConnected());
    ASSERT_EQ(VoltWatch::WiFiState::CONNECTED, wifi.getState());
    // Backoff resets to min (2000ms)
    ASSERT_EQ(uint32_t(2000), wifi.getCurrentBackoffIntervalMs());
    ASSERT_EQ(uint32_t(0), wifi.getReconnectAttempts());
}

TEST_CASE(WiFiManager, DisconnectAndForceReconnect) {
    VoltWatch::WiFiManager wifi;
    wifi.setMockMode(true);
    wifi.setMockConnected(true);
    wifi.init("OfficeWiFi", "Secret");
    wifi.update(100);
    ASSERT_TRUE(wifi.isConnected());

    // Disconnect
    wifi.disconnect();
    ASSERT_FALSE(wifi.isConnected());
    ASSERT_EQ(VoltWatch::WiFiState::DISCONNECTED, wifi.getState());

    // Force reconnect at t = 2000ms
    wifi.forceReconnect(2000);
    ASSERT_EQ(VoltWatch::WiFiState::CONNECTING, wifi.getState());

    wifi.update(2050);
    ASSERT_TRUE(wifi.isConnected());
}
