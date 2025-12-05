# Makefile for mod_event_agent
# Multi-driver event streaming module for FreeSWITCH

MODULE_NAME = mod_event_agent

# Compiler and flags
CC = gcc
CFLAGS = -fPIC -Wall -Werror -g -O2 -std=gnu99

# FreeSWITCH include directory (can be overridden)
FREESWITCH_INCLUDE_DIR ?= /tmp/freeswitch_headers

CFLAGS += -I$(FREESWITCH_INCLUDE_DIR)
CFLAGS += -I/tmp/freeswitch/libs/libteletone/src
CFLAGS += -I/usr/local/include
CFLAGS += -I./include

# Local NATS library
# Use shared library if available (for Docker), otherwise static
ifneq (,$(wildcard /usr/local/lib/libnats.so))
  NATS_LIB = -lnats
  NATS_RPATH =
else ifneq (,$(wildcard ./lib/nats/libnats.so))
  NATS_LIB = -L./lib/nats -lnats
  NATS_RPATH = -Wl,-rpath,./lib/nats
else
  NATS_LIB = ./lib/nats/libnats_static.a
  NATS_RPATH =
endif

# Default drivers (can be overridden: make WITH_KAFKA=1)
WITH_NATS ?= 1
WITH_KAFKA ?= 0
WITH_RABBITMQ ?= 0
WITH_REDIS ?= 0

# Source files
SOURCES = src/mod_event_agent.c \
          src/event_adapter.c \
          src/event_agent_config.c \
          src/command_handler.c \
          src/commands/command_core.c \
          src/commands/command_call.c \
          src/commands/command_api.c \
          src/commands/command_status.c \
          src/serialization.c \
          src/logger.c

# Driver sources
ifeq ($(WITH_NATS),1)
  CFLAGS += -DWITH_NATS=1
  SOURCES += src/drivers/driver_nats.c
  LDFLAGS += $(NATS_LIB) $(NATS_RPATH) -lpthread -lssl -lcrypto
endif

ifeq ($(WITH_KAFKA),1)
  CFLAGS += -DWITH_KAFKA=1
  SOURCES += src/drivers/driver_kafka.c
  LDFLAGS += -lrdkafka
endif

ifeq ($(WITH_RABBITMQ),1)
  CFLAGS += -DWITH_RABBITMQ=1
  SOURCES += src/drivers/driver_rabbitmq.c
  LDFLAGS += -lrabbitmq
endif

ifeq ($(WITH_REDIS),1)
  CFLAGS += -DWITH_REDIS=1
  SOURCES += src/drivers/driver_redis.c
  LDFLAGS += -lhiredis
endif

# Common libs
LDFLAGS += -L/usr/local/lib -lcjson

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Output
TARGET = $(MODULE_NAME).so

.PHONY: all clean install test compile-nats

all: $(TARGET)

nats:
	@echo "🔧 Compiling with NATS driver..."
	$(MAKE) clean
	$(MAKE) WITH_NATS=1
	@echo "✅ NATS driver compiled successfully"

$(TARGET): $(OBJECTS)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
	@echo "✅ Built: $(TARGET)"
	@echo "   Drivers: NATS=$(WITH_NATS) Kafka=$(WITH_KAFKA) RabbitMQ=$(WITH_RABBITMQ) Redis=$(WITH_REDIS)"

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)
	rm -f src/*~ src/drivers/*~
	rm -f tests/bin/*
	@echo "✅ Cleaned"

install: $(TARGET)
	@if [ -z "$(DESTDIR)" ]; then \
		echo "❌ Error: DESTDIR not set"; \
		echo "   Usage: make install DESTDIR=/usr/local/freeswitch/mod"; \
		exit 1; \
	fi
	install -D -m 755 $(TARGET) $(DESTDIR)/$(TARGET)
	@echo "✅ Installed to: $(DESTDIR)/$(TARGET)"

# Test programs
tests: tests/bin/service_a_nats tests/bin/service_b_nats tests/bin/simple_test

tests/bin/service_a_nats: tests/src/service_a_nats.c
	@mkdir -p tests/bin
	$(CC) $< -o $@ -I./include $(NATS_LIB) -lpthread -lssl -lcrypto
	@echo "✅ Built: $@"

tests/bin/service_b_nats: tests/src/service_b_nats.c
	@mkdir -p tests/bin
	$(CC) $< -o $@ -I./include $(NATS_LIB) -lpthread -lssl -lcrypto
	@echo "✅ Built: $@"

tests/bin/simple_test: tests/src/simple_test.c
	@mkdir -p tests/bin
	$(CC) $< -o $@ -I./include $(NATS_LIB) -lpthread -lssl -lcrypto
	@echo "✅ Built: $@"

# Docker targets
docker-up:
	@echo "🐳 Starting FreeSWITCH and NATS..."
	docker-compose -f docker-compose.dev.yaml --env-file .env.dev up -d nats_broker freeswitch
	@echo "✅ Containers started"
	@docker ps | grep -E "(nats|freeswitch)" || true

docker-down:
	@echo "🛑 Stopping containers..."
	docker-compose -f docker-compose.dev.yaml --env-file .env.dev down
	@echo "✅ Containers stopped"

docker-restart: docker-down docker-up

docker-logs:
	@echo "📋 FreeSWITCH logs:"
	docker logs agent_nats_dev_freeswitch --tail 50

docker-logs-nats:
	@echo "📋 NATS logs:"
	docker logs agent-dev-nats --tail 50

docker-shell:
	@echo "🐚 Entering FreeSWITCH container..."
	docker exec -it agent_nats_dev_freeswitch bash

docker-status:
	@echo "📊 Container status:"
	@docker ps | grep -E "NAMES|nats|freeswitch" || echo "No containers running"
	@echo ""
	@echo "📊 Port mappings:"
	@docker ps --format "table {{.Names}}\t{{.Ports}}" | grep -E "NAMES|nats|freeswitch" || true

# Test targets
test-compile:
	@echo "Testing compilation with all drivers..."
	$(MAKE) clean
	$(MAKE) WITH_NATS=1
	@echo "✅ NATS driver compiles"

test-services: tests
	@echo "Testing NATS services..."
	@./tests/bin/simple_test pub test.verify "Hello from Makefile"
	@echo "✅ Test services built successfully"

info:
	@echo "mod_event_agent - Build Configuration"
	@echo "====================================="
	@echo "Enabled Drivers:"
	@echo "  NATS:     $(WITH_NATS)"
	@echo "  Kafka:    $(WITH_KAFKA)"
	@echo "  RabbitMQ: $(WITH_RABBITMQ)"
	@echo "  Redis:    $(WITH_REDIS)"
	@echo ""
	@echo "Build flags:"
	@echo "  CFLAGS:  $(CFLAGS)"
	@echo "  LDFLAGS: $(LDFLAGS)"
	@echo ""
	@echo "Sources:"
	@for src in $(SOURCES); do echo "  $$src"; done

help:
	@echo "mod_event_agent - Available targets:"
	@echo ""
	@echo "Build targets:"
	@echo "  make              - Build module with NATS driver"
	@echo "  make compile-nats - Clean and build with NATS driver"
	@echo "  make clean        - Clean build files"
	@echo "  make tests        - Build test programs"
	@echo "  make install      - Install module (needs DESTDIR)"
	@echo ""
	@echo "Docker targets:"
	@echo "  make docker-up      - Start FreeSWITCH and NATS containers"
	@echo "  make docker-down    - Stop containers"
	@echo "  make docker-restart - Restart containers"
	@echo "  make docker-logs    - Show FreeSWITCH logs"
	@echo "  make docker-logs-nats - Show NATS logs"
	@echo "  make docker-shell   - Enter FreeSWITCH container"
	@echo "  make docker-status  - Show container status and ports"
	@echo ""
	@echo "Test targets:"
	@echo "  make test-compile - Test compilation"
	@echo "  make test-services - Test NATS services"
	@echo "  make info         - Show build configuration"
