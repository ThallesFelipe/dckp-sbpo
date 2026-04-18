BUILD_DIR := build
GENERATOR := Ninja
APP := $(BUILD_DIR)/dckp_sbpo
INSTANCE ?=

.PHONY: all debug release run run-release clean

all: debug

debug:
	cmake -S . -B $(BUILD_DIR) -G $(GENERATOR) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_CXX_COMPILER=/usr/bin/g++-14 \
		-DDCKP_ENABLE_SANITIZERS=ON \
		-DDCKP_ENABLE_HARDENING=ON
	cmake --build $(BUILD_DIR) -j

release:
	cmake -S . -B $(BUILD_DIR) -G $(GENERATOR) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_CXX_COMPILER=/usr/bin/g++-14 \
		-DDCKP_ENABLE_SANITIZERS=OFF \
		-DDCKP_ENABLE_HARDENING=ON
	cmake --build $(BUILD_DIR) -j

run: debug
	@if [ -n "$(INSTANCE)" ]; then \
		./$(APP) "$(INSTANCE)"; \
	else \
		./$(APP); \
	fi

run-release: release
	@if [ -n "$(INSTANCE)" ]; then \
		./$(APP) "$(INSTANCE)"; \
	else \
		./$(APP); \
	fi

clean:
	rm -rf $(BUILD_DIR)