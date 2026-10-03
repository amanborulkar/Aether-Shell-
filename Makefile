CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -std=c17 -g -D_POSIX_C_SOURCE=200809L

SHELL_SRCS  := $(filter-out src/daemon.c src/server.c, $(wildcard src/*.c))
SHELL_OBJS  := $(SHELL_SRCS:.c=.o)
DAEMON_SRCS := src/daemon.c src/server.c
DAEMON_OBJS := $(DAEMON_SRCS:.c=.o)

.PHONY: all clean run run-daemon test integration

all: aether aetherd

aether: $(SHELL_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

aetherd: $(DAEMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SHELL_OBJS): $(wildcard src/*.h)
$(DAEMON_OBJS): src/server.h

clean:
	rm -f $(SHELL_OBJS) $(DAEMON_OBJS) aether aetherd

run: aether
	./aether

run-daemon: aetherd
	./aetherd

test: aether
	@bash tests/run_tests.sh

integration: test