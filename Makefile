BUILD_DIR   := build
GENERATOR   := Ninja
APP_NAME    := dckp_sbpo
APP         := $(BUILD_DIR)/$(APP_NAME)
INSTANCE    ?=

CMAKE_FLAGS ?=

.PHONY: all help configure build test run clean

all: build

help:
	@echo "Supported workflow (WSL Ubuntu 24.04):"
	@echo "  make build                Configure and build into build/."
	@echo "  make test                 Build once in build/ and run CTest."
	@echo "  make run INSTANCE=path    Build once in build/ and run the CLI."
	@echo "  make clean                Remove build/."

configure:
	@command -v ninja >/dev/null 2>&1 || { \
		echo "Ninja is required in WSL Ubuntu 24.04. Install it with: sudo apt install ninja-build"; \
		exit 1; \
	}
	@if [ -f "$(BUILD_DIR)/CMakeCache.txt" ] && \
		! grep -q '^CMAKE_GENERATOR:INTERNAL=$(GENERATOR)$$' "$(BUILD_DIR)/CMakeCache.txt"; then \
		echo ">>> Recreating $(BUILD_DIR)/ to switch CMake generator to $(GENERATOR)."; \
		rm -rf "$(BUILD_DIR)"; \
	fi
	cmake -S . -B $(BUILD_DIR) -G "$(GENERATOR)" -DCMAKE_BUILD_TYPE=Release -DDCKP_ENABLE_SANITIZERS=OFF -DDCKP_ENABLE_HARDENING=ON $(CMAKE_FLAGS)

build: configure
	cmake --build $(BUILD_DIR) --parallel

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

run: build
	@if [ -n "$(INSTANCE)" ]; then \
		./$(APP) "$(INSTANCE)"; \
	else \
		./$(APP); \
	fi

clean:
	rm -rf $(BUILD_DIR)
