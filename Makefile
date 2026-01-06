# Makefile for mod_event_agent
# Multi-driver event streaming module for FreeSWITCH

MODULE_NAME = mod_event_agent

# Compiler and flags
CC = gcc
CFLAGS = -fPIC -Wall -Werror -g -O2 -std=gnu99

# FreeSWITCH include directory (can be overridden)
ifeq ($(OS),Windows_NT)
	DEFAULT_FS_HEADERS := C:/tmp/freeswitch_headers
	DEFAULT_FS_LIBTELETONE := C:/tmp/freeswitch/libs/libteletone/src
else
	DEFAULT_FS_HEADERS := /tmp/freeswitch_headers
	DEFAULT_FS_LIBTELETONE := /tmp/freeswitch/libs/libteletone/src
endif

FREESWITCH_INCLUDE_DIR ?= $(DEFAULT_FS_HEADERS)
FREESWITCH_LIBTELETONE_DIR ?= $(DEFAULT_FS_LIBTELETONE)

CFLAGS += -I$(FREESWITCH_INCLUDE_DIR)
CFLAGS += -I$(FREESWITCH_LIBTELETONE_DIR)
CFLAGS += -I/usr/local/include
CFLAGS += -I./include
CFLAGS += -I./src

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
          src/core/config.c \
          src/core/logger.c \
          src/events/adapter.c \
          src/events/serializer.c \
          src/dialplan/manager.c \
          src/dialplan/commands.c \
          src/commands/handler.c \
          src/commands/core.c \
          src/commands/call.c \
          src/commands/api.c \
		  src/commands/status.c \
		  src/validation/validation.c

# Driver sources
ifeq ($(WITH_NATS),1)
  CFLAGS += -DWITH_NATS=1
  SOURCES += src/drivers/nats.c
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

.PHONY: all clean install nats examples info help

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

# Docker targets
docker-build:
	@echo "🐳 Building mod_events_agent image..."
	docker build -t mod_events_agent:latest -f Dockerfile .
	@echo "✅ mod_events_agent image built"

docker-build-freeswitch: docker-build
	@echo "🐳 Building freeswitch-events-agent image..."
	docker build -t freeswitch-events-agent:latest -f Dockerfile.freeswitch .
	@echo "✅ freeswitch-events-agent image built"

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

# Examples
examples:
	@echo "📚 For examples, see examples/ directory"
	@echo "   cd examples && make help"

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
	@echo "Other:"
	@echo "  make info          - Show build configuration"
	@echo "  make examples      - Show how to run examples"
	@echo "                       (or: cd examples && make help)"
