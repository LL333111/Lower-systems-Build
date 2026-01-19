# Copyright notice: redistribution of this assignment code
# (including public GitHub repos) is strictly prohibited.
#
# Provides the following utilities:
# - make program
#       build programs in SRC_DIR, defaulted to `src`.
# - make synthetic_traces
#       build programs in `synthetic` and generates the corresponding `.mref` files.
#       see synthetic/Makefile
# - make trace-gen
#       Builds the components of the trace generator
# - make traces
#       Generates memory traces from default programs provided in `src` with default parameters.
#       Requires these programs and the components of the trace generator to be built beforehand.
# - make clean-traces
#       Removes all metadata and directories used in the creation of traces,
#       but not the components of the trace generator itself.

SRC_DIR ?= src

$(shell mkdir -p build)

BUILD_DIR := $(realpath build)

SRCS := $(shell find $(SRC_DIR) -name '*.c')
TARGETS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%)

CCFLAGS += -Og -Wall -Werror

# to disable debug build, run with NDEBUG=y make ...
ifeq ($(NDEBUG), y)
	CCFLAGS += -Ofast -DNDEBUG
else
	CCFLAGS += -ggdb3
endif

.PHONY: all traces trace-gen programs synthetic_traces clean clean-src clean-traces clean-trace-gen clean-synthetic realclean

all: programs synthetic_traces trace-gen

programs:
	@if [ -d "$(SRC_DIR)" ]; then \
		cd $(SRC_DIR) && BUILD_DIR=$(BUILD_DIR) $(MAKE); \
	else \
		echo "Warning: $(SRC_DIR) not found, skipping programs"; \
	fi

synthetic_traces:
	@if [ -d "synthetic" ]; then \
		cd synthetic && $(MAKE); \
	else \
		echo "Warning: synthetic directory not found, skipping synthetic_traces"; \
	fi

traces: trace-gen
	./run.sh matmul 100
	./run.sh repeatloop 30000 4
	./run.sh simpleloop_fork
	./run.sh reuse_scan
	./run.sh blocked 100 25

TG_TARGETS := simplify_trace
trace-gen: $(TG_TARGETS)

$(TG_TARGETS): %: %.c
	$(CC) $(CCFLAGS) $< -I. -o $@

realclean: clean clean-traces

# separated because generating traces might take a while
clean-traces:
	rm -rf runs
	rm -rf traces

clean: clean-src clean-trace-gen clean-synthetic

clean-src:
	cd $(SRC_DIR) && BUILD_DIR=$(BUILD_DIR) $(MAKE) clean

clean-trace-gen:
	rm -f $(TG_TARGETS)

clean-synthetic:
	cd synthetic && $(MAKE) clean
