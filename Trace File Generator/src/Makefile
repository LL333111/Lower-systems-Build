# Copyright notice: redistribution of this assignment code
# (including public GitHub repos) is strictly prohibited.
#
BUILD_DIR ?= build

SRCS := $(wildcard *.c)
TARGETS := $(SRCS:%.c=$(BUILD_DIR)/%)

CCFLAGS += -Og -Wall -Werror

all: $(TARGETS)

.PHONY: all

$(BUILD_DIR)/%:: %.c marker.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CCFLAGS) $< -I. -o $@

$(filter matmul blocked, $(TARGETS)) :$(BUILD_DIR)/%: %.c marker.h timer.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CCFLAGS) $< -I. -o $@

clean:
	rm -f $(TARGETS)
