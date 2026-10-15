CXX ?= clang++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -I../ -I../include
BIN_DIR = bin
TARGET = $(BIN_DIR)/run_tests

SRCS = test_main.cpp \
       test_moving_average.cpp \
       test_alert_manager.cpp \
       test_voltage_sensor.cpp \
       test_telemetry.cpp \
       test_outage_state_machine.cpp \
       test_pzem_manager.cpp \
       test_api_status.cpp \
       test_alert_payloads.cpp \
       test_outage_storage.cpp \
       test_wifi_manager.cpp \
       ../src/sensors/VoltageSensor.cpp \
       ../src/alerts/AlertManager.cpp \
       ../src/alerts/SMTPAlertDispatcher.cpp \
       ../src/storage/OutageStorage.cpp \
       ../src/network/WiFiManager.cpp \
       ../src/telemetry/TelemetryReporter.cpp \
       ../src/display/StatusIndicator.cpp \
       ../src/metrology/PZEMManager.cpp \
       ../src/statemachine/OutageStateMachine.cpp \
       ../src/web/WebDashboard.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

test: $(TARGET)
	@./$(TARGET)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all test clean
