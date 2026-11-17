CC ?= cc
AR ?= ar
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I.. -I.
LDLIBS += -lm
BUILD_DIR := build
LIB_OBJECTS := $(BUILD_DIR)/baseline_common.o $(BUILD_DIR)/dp_solver.o $(BUILD_DIR)/greedy_solver.o
TEST_BINS := $(BUILD_DIR)/test_baselines $(BUILD_DIR)/test_decimal_boundary

.PHONY: all library test sanitize clean

all: library

library: $(BUILD_DIR)/librelief_baselines.a

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/baseline_common.o: baseline_common.c baseline_common.h ../relief_schema.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/dp_solver.o: dp_solver.c dp_solver.h baseline_common.h ../relief_schema.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/greedy_solver.o: greedy_solver.c greedy_solver.h baseline_common.h ../relief_schema.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/librelief_baselines.a: $(LIB_OBJECTS)
	$(AR) rcs $@ $^

$(BUILD_DIR)/test_baselines: ../tests/test_baselines.c $(LIB_OBJECTS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/test_decimal_boundary: ../tests/test_decimal_boundary.c $(LIB_OBJECTS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

test: $(TEST_BINS)
	./$(BUILD_DIR)/test_baselines
	./$(BUILD_DIR)/test_decimal_boundary

sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS="-std=c11 -O1 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS="-fsanitize=address,undefined" test

clean:
	rm -rf $(BUILD_DIR)
