# Tests Makefile — builds the standalone integration test binaries.
#
# These binaries talk to a running FS that has mod_event_agent loaded
# and connected to the same NATS broker. The harness in run.sh boots
# the FS+module image (via Dockerfile.freeswitch) before invoking
# them, so that "make check" in CI is one command.
#
# Two ways to provision the libnats client lib at link time:
#   - The bundled static archive at ../lib/nats/libnats_static.a (the
#     same one the module uses). Default — zero system deps required.
#   - A system-wide install (-lnats from /usr/local/lib). Useful for
#     running tests inside a container that already has libnats.
#
# Override LDFLAGS_NATS at make time to switch:
#     make NATS_LIBS='-L/usr/local/lib -lnats'

CC      ?= gcc
CFLAGS  += -Wall -Werror -O2 -std=gnu99 -I../include
LDFLAGS_NATS ?= ../lib/nats/libnats_static.a -lpthread -lssl -lcrypto

BIN_DIR := bin
SRCS    := \
    src/show_modules_test.c \
    src/test_size_limit.c \
    src/test_denylist.c \
    src/test_validation.c

TESTS   := $(patsubst src/%.c,$(BIN_DIR)/%,$(SRCS))

.PHONY: all clean check

all: $(TESTS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Each test is a single-file binary. The pattern rule below covers all
# four with one recipe; if a test ever needs extra .c files, give it
# its own explicit rule above the pattern.
$(BIN_DIR)/%: src/%.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_NATS)

# `make check` runs every binary sequentially through run.sh, which
# also brings up the docker stack and points the env vars at the right
# broker.
check: all
	./run.sh

clean:
	rm -rf $(BIN_DIR)
