.PHONY: all build build-tests build-bench test bench bench-compare coverage coverage-clean clean format help

BUILD_TYPE ?= Debug
BUILD_DIR := build/$(shell echo $(BUILD_TYPE) | tr '[:upper:]' '[:lower:]')
COVERAGE_BUILD_DIR := build/coverage
COVERAGE_DIR := coverage

all: build

build:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	cmake --build $(BUILD_DIR) -j

build-tests:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DNEXUSALLOC_BUILD_TESTS=ON
	cmake --build $(BUILD_DIR) -j

build-bench:
ifeq ($(shell echo $(BUILD_TYPE) | tr '[:upper:]' '[:lower:]'),debug)
	@echo "\033[33mWarning: Benchmarks should not be run in Debug mode. Using Release instead.\033[0m"
	cmake -B build/release -DCMAKE_BUILD_TYPE=Release -DNEXUSALLOC_BUILD_BENCHMARKS=ON
	cmake --build build/release -j
else
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DNEXUSALLOC_BUILD_BENCHMARKS=ON
	cmake --build $(BUILD_DIR) -j
endif

test: build-tests
	cd $(BUILD_DIR) && ctest --output-on-failure

bench: build-bench
ifeq ($(shell echo $(BUILD_TYPE) | tr '[:upper:]' '[:lower:]'),debug)
	build/release/benchmarks/bench_allocator
else
	$(BUILD_DIR)/benchmarks/bench_allocator
endif

# Run comparison benchmarks against jemalloc/tcmalloc
bench-compare: build-bench
ifeq ($(shell echo $(BUILD_TYPE) | tr '[:upper:]' '[:lower:]'),debug)
	build/release/benchmarks/bench_comparison
else
	$(BUILD_DIR)/benchmarks/bench_comparison
endif

coverage:
	@rm -rf $(COVERAGE_DIR)
	@find $(COVERAGE_BUILD_DIR) -name '*.gcda' -delete 2>/dev/null || true
	cmake -B $(COVERAGE_BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -DNEXUSALLOC_BUILD_TESTS=ON \
		-DCMAKE_CXX_FLAGS="--coverage -fprofile-arcs -ftest-coverage"
	cmake --build $(COVERAGE_BUILD_DIR) --target nexusalloc_tests -j
	cd $(COVERAGE_BUILD_DIR) && ctest --output-on-failure
	@mkdir -p $(COVERAGE_DIR)
	@cd $(COVERAGE_BUILD_DIR)/tests/CMakeFiles/nexusalloc_tests.dir && gcov -p *.gcda >/dev/null 2>&1
	@find $(COVERAGE_BUILD_DIR) -name '*#include#nexusalloc#*.gcov' -exec mv {} $(COVERAGE_DIR)/ \;
	@echo ""
	@echo "=== Coverage Summary (include/nexusalloc/) ==="
	@for f in $(COVERAGE_DIR)/*.gcov; do \
		filename=$$(basename "$$f" | sed 's/#/\//g' | sed 's/\.gcov$$//'); \
		executed=$$(grep -E "^\s+[0-9]+:" "$$f" 2>/dev/null | wc -l); \
		not_executed=$$(grep -E "^\s+#####:" "$$f" 2>/dev/null | wc -l); \
		lines=$$((executed + not_executed)); \
		if [ $$lines -gt 0 ]; then \
			pct=$$((executed * 100 / lines)); \
			printf "  %-60s %3d%% (%d/%d lines)\n" "$$filename" $$pct $$executed $$lines; \
		fi; \
	done
	@echo ""
	@echo "Coverage files generated in $(COVERAGE_DIR)/"

coverage-clean:
	rm -rf $(COVERAGE_DIR)
	rm -rf $(COVERAGE_BUILD_DIR)

format:
	find include src tests benchmarks -name '*.hpp' -o -name '*.cpp' | xargs clang-format -i

clean: coverage-clean
	rm -rf build/
