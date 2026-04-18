BUILD_DIR   ?= build
GENERATOR   ?= Ninja
APP_NAME    := dckp_sbpo
APP         := $(BUILD_DIR)/$(APP_NAME)
INSTANCE    ?=

# Compiler selection is left to CMake by default. Set CXX in the environment
# (e.g. `CXX=g++-14 make debug`) to override, or pass -DCMAKE_CXX_COMPILER=...
# via CMAKE_FLAGS.
CMAKE_FLAGS ?=

.PHONY: all debug release run run-release test clean help

all: debug

help:
	@echo "Targets:"
	@echo "  debug        Configure+build Debug (sanitizers + hardening)."
	@echo "  release      Configure+build Release (hardening, no sanitizers)."
	@echo "  test         Build Debug and run the test suite via CTest."
	@echo "  run          Build Debug and run the CLI. Set INSTANCE=<path>."
	@echo "  run-release  Build Release and run the CLI. Set INSTANCE=<path>."
	@echo "  clean        Remove the build directory."

debug:
	cmake -S . -B $(BUILD_DIR) -G $(GENERATOR) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DDCKP_ENABLE_SANITIZERS=ON \
		-DDCKP_ENABLE_HARDENING=ON \
		$(CMAKE_FLAGS)
	cmake --build $(BUILD_DIR) -j

release:
	cmake -S . -B $(BUILD_DIR) -G $(GENERATOR) \
		-DCMAKE_BUILD_TYPE=Release \
		-DDCKP_ENABLE_SANITIZERS=OFF \
		-DDCKP_ENABLE_HARDENING=ON \
		$(CMAKE_FLAGS)
	cmake --build $(BUILD_DIR) -j

test: debug
	ctest --test-dir $(BUILD_DIR) --output-on-failure

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
