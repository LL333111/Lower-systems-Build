CC = gcc
CFLAGS := -g3 -Wall -Wextra -Werror -D_GNU_SOURCE $(CFLAGS)
LDFLAGS := $(LDFLAGS) -lm
ARCH := $(shell uname -m)

OBJECTS := rr.o rand.o s2q.o clock.o pagetable.o sim.o swap.o malloc369.o \
		   coremap.o tlb.o multiprocessing.o ptrarray.o
DIRNAME := $(notdir $(CURDIR))
ZIPFILE := a3-$(DIRNAME).zip

ifeq ($(ARCH),x86_64)
	# AVX2 will be automatically enabled only if the compiling system
	# supports it. Code will be tuned for maximum performance on the
	# testing server.
	CFLAGS := $(CFLAGS) -mavx2 -mtune=znver3
endif

.PHONY: all clean zip

all: sim convert

sim: $(OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)

convert: convert.c
	$(CC) -Ofast -march=native $^ -o $@

-include $(OBJECTS:.o=.d)

%.o: %.c
	$(CC) $< -o $@ -c -MMD $(CFLAGS)

clean:
	rm -f $(OBJECTS) $(OBJECTS:.o=.d) sim swapfile.*
	rm -f convert

# creates a zip file in the parent directory
zip: clean
	zip -r ../$(ZIPFILE) .