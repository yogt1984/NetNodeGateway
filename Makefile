# NetNodeGateway Makefile
# Wraps CMake build system for convenience

BUILD_DIR := build
CMAKE := cmake
NPROC := $(shell nproc 2>/dev/null || echo 4)

.PHONY: all build run run-demo run-gateway run-sensor test clean rebuild help

# Default target: build and run demo
all: build run-demo

# Build the project
build:
	@echo "=== Building NetNodeGateway ==="
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) -DCMAKE_BUILD_TYPE=Debug .. && $(CMAKE) --build . -j$(NPROC)
	@echo "=== Build complete ==="

# Run the full demo (sensor + gateway in parallel, then show output)
run-demo: build
	@echo ""
	@echo "=============================================="
	@echo "  NetNodeGateway Demo - UDP/TCP Closed Loop"
	@echo "=============================================="
	@echo "  Rheinmetall Interview - 6.2.26"
	@echo "=============================================="
	@echo ""
	@echo "[Gateway] Starting UDP listener on port 5000 (DEBUG mode)..."
	@$(BUILD_DIR)/src/gateway/gateway --port 5000 --log-level DEBUG --no-crc & \
	GATEWAY_PID=$$!; \
	sleep 0.5; \
	echo ""; \
	echo "[Sensor] Starting simulator with fault injection..."; \
	echo "         (5% packet loss, 3% reorder, 2% duplicate)"; \
	echo ""; \
	$(BUILD_DIR)/src/sensor_sim/sensor_sim \
		--host 127.0.0.1 \
		--port 5000 \
		--profile patrol \
		--rate 20 \
		--duration 9 \
		--loss 5 \
		--reorder 3 \
		--duplicate 2; \
	sleep 0.5; \
	kill $$GATEWAY_PID 2>/dev/null; \
	wait $$GATEWAY_PID 2>/dev/null; \
	echo ""; \
	echo "=== Demo complete ==="

# Run gateway only (listens for UDP telemetry)
run-gateway: build
	@echo "Starting Gateway on UDP port 5000 (Ctrl+C to stop)..."
	@$(BUILD_DIR)/src/gateway/gateway --port 5000 --log-level DEBUG --no-crc

# Run sensor simulator only
run-sensor: build
	@echo "Starting Sensor Simulator..."
	@$(BUILD_DIR)/src/sensor_sim/sensor_sim --host 127.0.0.1 --port 5000 --rate 20 --duration 5

# Run replay tool (requires recorded frames)
run-replay: build
	@echo "Starting Replay Engine..."
	@$(BUILD_DIR)/src/replay/replay --help

# Run all tests
test: build
	@echo "=== Running Tests ==="
	@cd $(BUILD_DIR) && ctest --output-on-failure -j$(NPROC)

# Clean build artifacts
clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)
	@echo "Clean complete."

# Full rebuild
rebuild: clean build

# Help target
help:
	@echo "NetNodeGateway Makefile Targets:"
	@echo ""
	@echo "  make          - Build and run demo (default)"
	@echo "  make build    - Build the project"
	@echo "  make run-demo - Run full demo (gateway + sensor)"
	@echo "  make run-gateway - Run gateway only"
	@echo "  make run-sensor  - Run sensor simulator only"
	@echo "  make test     - Run all unit tests"
	@echo "  make clean    - Remove build directory"
	@echo "  make rebuild  - Clean and rebuild"
	@echo "  make help     - Show this help"
