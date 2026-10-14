BUILD_DIR ?= build
BUILD_TYPE ?= Debug

.PHONY: all configure run test clean

all: configure
	cmake --build $(BUILD_DIR)

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

run: all
	./$(BUILD_DIR)/QueryFlow

test: all
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	cmake -E rm -rf $(BUILD_DIR)
