CC ?= cc
CLANG ?= clang

CPPFLAGS := -I.
COMMON_WARNINGS := -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion \
	-Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes \
	-Wmissing-prototypes -Werror
CFLAGS := -std=c11 -O2 $(COMMON_WARNINGS)
SANITIZER_FLAGS := -O1 -g -fsanitize=address,undefined,pointer-compare,pointer-subtract \
	-fno-omit-frame-pointer
ASAN_OPTIONS := detect_invalid_pointer_pairs=2:detect_leaks=0:halt_on_error=1
BUILD_DIR := build

.PHONY: all test unit guard fuzz compile-fail header-compat memory sanitizer \
	valgrind optimization check benchmark clean

all: test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/unit-gcc: String.c String.h tests/test_string.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) String.c tests/test_string.c -o $@

$(BUILD_DIR)/unit-clang: String.c String.h tests/test_string.c | $(BUILD_DIR)
	$(CLANG) $(CPPFLAGS) $(CFLAGS) String.c tests/test_string.c -o $@

$(BUILD_DIR)/guard-gcc: String.c String.h tests/test_guard_pages.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) String.c tests/test_guard_pages.c -o $@

$(BUILD_DIR)/guard-clang: String.c String.h tests/test_guard_pages.c | $(BUILD_DIR)
	$(CLANG) $(CPPFLAGS) $(CFLAGS) String.c tests/test_guard_pages.c -o $@

$(BUILD_DIR)/fuzz-gcc: String.c String.h tests/fuzz_string.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) String.c tests/fuzz_string.c -o $@

$(BUILD_DIR)/fuzz-clang: String.c String.h tests/fuzz_string.c | $(BUILD_DIR)
	$(CLANG) $(CPPFLAGS) $(CFLAGS) String.c tests/fuzz_string.c -o $@

unit: $(BUILD_DIR)/unit-gcc $(BUILD_DIR)/unit-clang
	$(BUILD_DIR)/unit-gcc
	$(BUILD_DIR)/unit-clang

guard: $(BUILD_DIR)/guard-gcc $(BUILD_DIR)/guard-clang
	$(BUILD_DIR)/guard-gcc
	$(BUILD_DIR)/guard-clang

fuzz: $(BUILD_DIR)/fuzz-gcc $(BUILD_DIR)/fuzz-clang
	$(BUILD_DIR)/fuzz-gcc 100000
	$(BUILD_DIR)/fuzz-clang 100000

compile-fail: | $(BUILD_DIR)
	@if $(CC) $(CPPFLAGS) -std=c11 $(COMMON_WARNINGS) \
		-c tests/compile_fail_strscpy_auto_pointer.c \
		-o $(BUILD_DIR)/compile-fail-gcc.o >$(BUILD_DIR)/compile-fail-gcc.log 2>&1; then \
		echo "FAIL: GCC accepted a pointer passed to strscpy_auto"; \
		exit 1; \
	else \
		grep -q "destination must be an array, not a pointer" \
			$(BUILD_DIR)/compile-fail-gcc.log || { \
			cat $(BUILD_DIR)/compile-fail-gcc.log; \
			echo "FAIL: GCC rejected strscpy_auto for the wrong reason"; \
			exit 1; \
		}; \
		echo "PASS: GCC rejected pointer passed to strscpy_auto"; \
	fi
	@if $(CLANG) $(CPPFLAGS) -std=c11 $(COMMON_WARNINGS) \
		-c tests/compile_fail_strscpy_auto_pointer.c \
		-o $(BUILD_DIR)/compile-fail-clang.o >$(BUILD_DIR)/compile-fail-clang.log 2>&1; then \
		echo "FAIL: Clang accepted a pointer passed to strscpy_auto"; \
		exit 1; \
	else \
		grep -q "destination must be an array, not a pointer" \
			$(BUILD_DIR)/compile-fail-clang.log || { \
			cat $(BUILD_DIR)/compile-fail-clang.log; \
			echo "FAIL: Clang rejected strscpy_auto for the wrong reason"; \
			exit 1; \
		}; \
		echo "PASS: Clang rejected pointer passed to strscpy_auto"; \
	fi
	@for source in tests/compile_fail_strtomem_auto_pointer.c \
		tests/compile_fail_strtomem_pad_auto_pointer.c; do \
		name=$$(basename $$source .c); \
		if $(CC) $(CPPFLAGS) -std=c11 $(COMMON_WARNINGS) -c $$source \
			-o $(BUILD_DIR)/$$name-gcc.o >$(BUILD_DIR)/$$name-gcc.log 2>&1; then \
			echo "FAIL: GCC accepted pointer in $$source"; exit 1; \
		else \
			grep -q "destination must be an array, not a pointer" \
				$(BUILD_DIR)/$$name-gcc.log || { \
				cat $(BUILD_DIR)/$$name-gcc.log; \
				echo "FAIL: GCC rejected $$source for the wrong reason"; \
				exit 1; \
			}; \
			echo "PASS: GCC rejected pointer in $$source"; \
		fi; \
		if $(CLANG) $(CPPFLAGS) -std=c11 $(COMMON_WARNINGS) -c $$source \
			-o $(BUILD_DIR)/$$name-clang.o >$(BUILD_DIR)/$$name-clang.log 2>&1; then \
			echo "FAIL: Clang accepted pointer in $$source"; exit 1; \
		else \
			grep -q "destination must be an array, not a pointer" \
				$(BUILD_DIR)/$$name-clang.log || { \
				cat $(BUILD_DIR)/$$name-clang.log; \
				echo "FAIL: Clang rejected $$source for the wrong reason"; \
				exit 1; \
			}; \
			echo "PASS: Clang rejected pointer in $$source"; \
		fi; \
	done

header-compat: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c11 $(COMMON_WARNINGS) \
		-c tests/test_header_compat.c -o $(BUILD_DIR)/header-system-first-gcc.o
	$(CC) $(CPPFLAGS) -std=c11 $(COMMON_WARNINGS) -DSTRING_HEADER_FIRST \
		-c tests/test_header_compat.c -o $(BUILD_DIR)/header-project-first-gcc.o
	$(CLANG) $(CPPFLAGS) -std=c11 $(COMMON_WARNINGS) \
		-c tests/test_header_compat.c -o $(BUILD_DIR)/header-system-first-clang.o
	$(CLANG) $(CPPFLAGS) -std=c11 $(COMMON_WARNINGS) -DSTRING_HEADER_FIRST \
		-c tests/test_header_compat.c -o $(BUILD_DIR)/header-project-first-clang.o

test: unit guard fuzz compile-fail header-compat

$(BUILD_DIR)/sanitizer-gcc: String.c String.h tests/fuzz_string.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c11 $(COMMON_WARNINGS) $(SANITIZER_FLAGS) \
		String.c tests/fuzz_string.c -o $@

$(BUILD_DIR)/sanitizer-clang: String.c String.h tests/fuzz_string.c | $(BUILD_DIR)
	$(CLANG) $(CPPFLAGS) -std=c11 $(COMMON_WARNINGS) $(SANITIZER_FLAGS) \
		String.c tests/fuzz_string.c -o $@

sanitizer: $(BUILD_DIR)/sanitizer-gcc $(BUILD_DIR)/sanitizer-clang
	ASAN_OPTIONS=$(ASAN_OPTIONS) $(BUILD_DIR)/sanitizer-gcc 20000
	ASAN_OPTIONS=$(ASAN_OPTIONS) $(BUILD_DIR)/sanitizer-clang 20000

$(BUILD_DIR)/valgrind: String.c String.h tests/fuzz_string.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c11 -O1 -g $(COMMON_WARNINGS) \
		String.c tests/fuzz_string.c -o $@

valgrind: $(BUILD_DIR)/valgrind
	valgrind --quiet --leak-check=full --show-leak-kinds=all \
		--track-origins=yes --error-exitcode=99 $(BUILD_DIR)/valgrind 5000

memory: guard sanitizer valgrind

optimization: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c11 -O0 $(COMMON_WARNINGS) \
		String.c tests/test_string.c -o $(BUILD_DIR)/unit-gcc-o0
	$(BUILD_DIR)/unit-gcc-o0
	$(CC) $(CPPFLAGS) -std=c11 -O3 -flto $(COMMON_WARNINGS) \
		String.c tests/test_string.c -o $(BUILD_DIR)/unit-gcc-o3-lto
	$(BUILD_DIR)/unit-gcc-o3-lto
	$(CLANG) $(CPPFLAGS) -std=c11 -O3 $(COMMON_WARNINGS) \
		String.c tests/test_string.c -o $(BUILD_DIR)/unit-clang-o3
	$(BUILD_DIR)/unit-clang-o3
	$(CC) $(CPPFLAGS) -std=c11 -O2 -D_FORTIFY_SOURCE=3 \
		-fstack-protector-strong -fPIE -pie $(COMMON_WARNINGS) \
		String.c tests/test_string.c -o $(BUILD_DIR)/unit-hardened
	$(BUILD_DIR)/unit-hardened

check: test memory optimization

$(BUILD_DIR)/benchmark: String.c String.h tests/benchmark_string.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) String.c tests/benchmark_string.c -ldl -o $@

benchmark: $(BUILD_DIR)/benchmark
	$(BUILD_DIR)/benchmark

clean:
	rm -rf $(BUILD_DIR)
