# Copyright notice: redistribution of this assignment code
# (including public GitHub repos) is strictly prohibited.

CSRCS := $(wildcard *.c)

C_TARGETS := $(CSRCS:%.c=%)

CCFLAGS += -Og -g3 -Wall -Werror

.PHONY: all refs clean clean-refs

all: $(C_TARGETS) refs

$(C_TARGETS): %: %.c
	$(CC) $(CCFLAGS) $< -o $@

refs: $(C_TARGETS)
	for exe in $^; do	\
		./$$exe;	\
	done

clean: clean-refs
	rm -f $(C_TARGETS)

clean-refs:
	rm -f *.mref
