#
# Makefile for user-level threading library
#
# Author: Kuei Sun, Angela Demke Brown, Ashvin Goel
# E-mail: kuei.sun@mail.utoronto.ca
#
# University of Toronto, 2025
#

# debug or release
CONF := debug
CFLAGS := -Wall -Wextra -Werror -D_GNU_SOURCE

ifeq ($(CONF),debug)
CFLAGS   += -g -O0 -ggdb
else ifeq ($(CONF),release)
CFLAGS   += -O3
else
$(error CONF must be either debug or release)
endif

TARFILE := $(notdir $(shell pwd)).tar
TESTSRC := $(wildcard test/*.c)
SOURCES := $(wildcard *.c)
COMMON_OBJECTS := $(SOURCES:.c=.o)
TARGET := $(TESTSRC:.c=)
DEPEND := .depend

# Make sure that 'all' is the first target
all: $(DEPEND) $(TARGET)

zip: realclean
	tar cvf $(TARFILE) *.c *.h Makefile

clean:
	rm -rf *.stackdump *.o test/*.o test/*.exe *.d $(TARGET) $(DEPEND) *.exe

realclean: clean
	rm -rf *~ *.log *.out *.tar scripts/__pycache__

$(DEPEND):
	$(CC) -MM *.c > $(DEPEND)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET) : % : %.o $(COMMON_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $< $(COMMON_OBJECTS)

test/%.o: CFLAGS += -Wno-cast-function-type -Wno-deprecated-declarations

ifeq ($(DEPEND),$(wildcard $(DEPEND)))
include $(DEPEND)
endif
