.PHONY: all build test bench clean format

BUILD_TYPE ?= Debug
BUILD_DIR := build/$(shell echo $(BUILD_TYPE) | tr '[:upper:]' '[:lower:]')

all: build

build:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	cmake --build $(BUILD_DIR) -j

format:
	find include src tests benchmarks -name '*.hpp' -o -name '*.cpp' | xargs clang-format -i

clean:
	rm -rf build/
