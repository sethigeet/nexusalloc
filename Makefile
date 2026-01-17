.PHONY: all build test bench clean format

BUILD_TYPE ?= Debug
BUILD_DIR := build/$(shell echo $(BUILD_TYPE) | tr '[:upper:]' '[:lower:]')

all: build

build:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	cmake --build $(BUILD_DIR) -j

test: build
	cd $(BUILD_DIR) && ctest --output-on-failure

bench: build
	$(BUILD_DIR)/benchmarks/bench_allocator

format:
	find include src tests benchmarks -name '*.hpp' -o -name '*.cpp' | xargs clang-format -i

clean:
	rm -rf build/
