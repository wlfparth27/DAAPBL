CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
LDLIBS ?= -lm
BUILD_DIR := build

.PHONY: all test-sanitizer test clean

all: $(BUILD_DIR)/daa_branch_bound test-sanitizer

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/test_sanitizer: test_sanitizer.c sanitizer.c relief_schema.c dataset_generator.c sanitizer.h relief_schema.h dataset_generator.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I. test_sanitizer.c sanitizer.c relief_schema.c dataset_generator.c $(LDLIBS) -o $@

$(BUILD_DIR)/daa_branch_bound: DAA_PBL.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) DAA_PBL.c $(LDLIBS) -o $@

test-sanitizer: $(BUILD_DIR)/test_sanitizer
	./$(BUILD_DIR)/test_sanitizer

test: $(BUILD_DIR)/daa_branch_bound test-sanitizer
	$(MAKE) -C baselines test

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C baselines clean
