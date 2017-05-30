default: build/qlprint

# Get rid of most of the implicit rules by clearing the .SUFFIXES target
.SUFFIXES:
# Get rid of the auto-checkout from old version control systems rules
%: %,v
%: RCS/%,v
%: RCS/%
%: s.%
%: SCCS/s.%


CFLAGS=-std=c11 -Wall -Wextra -g -Iinclude -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809 $(shell pkg-config --cflags libpng)
LDFLAGS=$(shell pkg-config --libs libpng)

OBJS=$(addprefix build/, \
	main.o \
	ql.o \
	loadpng.o \
)

vpath %.c src

build/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

build/qlprint: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJS): $(wildcard include/*) Makefile

.PHONY: clean
clean:
	-rm -f build/*

