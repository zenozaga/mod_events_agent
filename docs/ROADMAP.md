# ROADMAP - mod_event_agent Drivers

Driver development roadmap for `mod_event_agent`.

---

## 📋 Current Status

| Driver | Status | Version | Features | Notes |
|--------|--------|---------|----------------|-------|
| **NATS** | ✅ **Complete** | 1.0 | Request-Reply, Connection pooling, Auto-reconnect | Production-ready |
| **Kafka** | 🚧 Stub | 0.1 | Interface defined | Requires implementation |
| **RabbitMQ** | 🚧 Stub | 0.1 | Interface defined | Requires implementation |
| **Redis** | 🚧 Stub | 0.1 | Interface defined | Requires implementation |

---

## ✅ NATS Driver (Complete)

### Implemented Features

- ✅ **Connection**: Initialization with configurable URL
- ✅ **Request-Reply**: Synchronous commands with responses
- ✅ **Fire-and-Forget**: Asynchronous commands without response
- ✅ **Auto-Reconnect**: Automatic reconnection with exponential backoff
- ✅ **Health Check**: Connection status verification
- ✅ **Node ID**: Identification in multi-node clusters
- ✅ **JSON Serialization**: Structured payloads
- ✅ **Error Handling**: Robust error management
- ✅ **Statistics**: Counters for requests/successes/failures
- ✅ **Performance**: ~10,000 req/s, <1ms local latency

### Configuration

```xml
<param name="driver" value="nats"/>
<param name="url" value="nats://localhost:4222"/>
<param name="node-id" value="fs-node-01"/>
<param name="nats-timeout" value="5000"/>
<param name="nats-max-reconnect" value="60"/>
<param name="nats-reconnect-wait" value="2000"/>
```

### Dependencies

- **NATS C Client**: v3.8.2
- **Library**: `lib/nats/libnats.so` (shared) or `lib/nats/libnats_static.a` (static)
- **Headers**: Included in project

### Testing

```bash
# Compile tests
cd tests && make

# Basic test
LD_LIBRARY_PATH=../lib/nats ./bin/service_a_nats '{"command":"status"}'

# Stress test
for i in {1..1000}; do
  LD_LIBRARY_PATH=../lib/nats ./bin/simple_test req freeswitch.api '{"command":"version"}'
done
```

---

## 🚧 Driver Kafka (Roadmap)

### Objetivo

Soporte para Apache Kafka como backend de mensajería, permitiendo:
- Event streaming masivo
- Persistencia de mensajes
- Procesamiento de logs históricos
- Integración con ecosistema Big Data

### Características Planeadas

- 🔲 **Producer**: Publicación de comandos y eventos
- 🔲 **Consumer**: Recepción de comandos desde topics
- 🔲 **Partitioning**: Distribución por node_id
- 🔲 **Offset Management**: Control de posición de lectura
- 🔲 **Batch Processing**: Agrupación de mensajes
- 🔲 **Compression**: Gzip/Snappy/LZ4
- 🔲 **Schema Registry**: Integración con Confluent Schema Registry

### Configuración Propuesta

```xml
<param name="driver" value="kafka"/>
<param name="url" value="localhost:9092"/>
<param name="node-id" value="fs-node-01"/>
<param name="kafka-topic-commands" value="freeswitch.commands"/>
<param name="kafka-topic-events" value="freeswitch.events"/>
<param name="kafka-consumer-group" value="freeswitch-group"/>
<param name="kafka-compression" value="snappy"/>
<param name="kafka-batch-size" value="16384"/>
```

### Required Dependencies

- **librdkafka**: C/C++ Apache Kafka client
- **Version**: >=1.9.0
- **Installation**: `apt install librdkafka-dev` or compile from source

### Driver Interface

```c
// driver_kafka.c - Methods to implement

switch_status_t kafka_init(const char *brokers, const char *node_id) {
    // 1. Create Kafka configuration
    // 2. Initialize producer
    // 3. Initialize consumer
    // 4. Subscribe to command topic
    // 5. Start polling thread
}

switch_status_t kafka_subscribe_commands(command_callback_t callback) {
    // 1. Consumer poll loop
    // 2. Deserialize message
    // 3. Invoke callback with JSON
}

switch_status_t kafka_send_command_response(const char *reply_subject, 
                                            const char *json_response) {
    // 1. Determine partition key (node_id)
    // 2. Produce message to response topic
    // 3. Flush if batch complete
}

switch_status_t kafka_publish_event(const char *subject, const char *json_payload) {
    // 1. Map subject to topic
    // 2. Produce event
}

void kafka_shutdown(void) {
    // 1. Flush pending messages
    // 2. Destroy producer
    // 3. Destroy consumer
    // 4. Cleanup threads
}

switch_bool_t kafka_is_connected(void) {
    // Verify producer and consumer status
}
```

### Testing Plan

```bash
# Start local Kafka
docker run -d --name kafka -p 9092:9092 apache/kafka:latest

# Basic test
./tests/bin/kafka_test_client '{"command":"status"}'

# Throughput test
./tests/bin/kafka_stress_test --messages 10000 --concurrency 50
```

### Contribution

To implement the Kafka driver:
1. Fork the repository
2. Copy `src/drivers/driver_nats.c` → `src/drivers/driver_kafka.c`
3. Implement functions according to interface above
4. Add `WITH_KAFKA=yes` in Makefile
5. Create tests in `tests/kafka_test_client.c`
6. Document in this ROADMAP
7. Submit PR

---

## 🚧 Driver RabbitMQ (Roadmap)

### Objective

Support for RabbitMQ as messaging backend, allowing:
- Enterprise message queuing
- Complex routing (exchanges, bindings)
- Delivery guarantees (acks, confirms)
- Integration with existing systems

### Planned Features

- 🔲 **Publisher**: Publishing to exchanges
- 🔲 **Consumer**: Consuming from queues
- 🔲 **Routing**: Topic exchanges with routing keys
- 🔲 **Acknowledgments**: Manual/automatic acks
- 🔲 **Publisher Confirms**: Delivery confirmation
- 🔲 **Prefetch**: Flow control
- 🔲 **Dead Letter**: Failed message handling

### Proposed Configuration

```xml
<param name="driver" value="rabbitmq"/>
<param name="url" value="amqp://localhost:5672"/>
<param name="node-id" value="fs-node-01"/>
<param name="rabbitmq-vhost" value="/"/>
<param name="rabbitmq-exchange" value="freeswitch"/>
<param name="rabbitmq-queue-commands" value="freeswitch.commands"/>
<param name="rabbitmq-routing-key" value="freeswitch.#"/>
<param name="rabbitmq-prefetch" value="10"/>
```

### Required Dependencies

- **librabbitmq**: RabbitMQ C client (rabbitmq-c)
- **Version**: >=0.11.0
- **Installation**: `apt install librabbitmq-dev`

### Driver Interface

```c
// driver_rabbitmq.c - Methods to implement

switch_status_t rabbitmq_init(const char *url, const char *node_id) {
    // 1. Connect to RabbitMQ
    // 2. Open channel
    // 3. Declare exchange
    // 4. Declare queue
    // 5. Bind queue to exchange
    // 6. Start consumer
}

switch_status_t rabbitmq_subscribe_commands(command_callback_t callback) {
    // 1. Basic.Consume on queue
    // 2. Reception loop
    // 3. Invoke callback
    // 4. Basic.Ack
}

switch_status_t rabbitmq_send_command_response(const char *reply_subject, 
                                                const char *json_response) {
    // 1. Basic.Publish with reply-to and correlation-id
    // 2. Publisher confirm
}

switch_status_t rabbitmq_publish_event(const char *subject, const char *json_payload) {
    // 1. Map subject to routing key
    // 2. Basic.Publish to exchange
}

void rabbitmq_shutdown(void) {
    // 1. Cancel consumer
    // 2. Close channel
    // 3. Close connection
}

switch_bool_t rabbitmq_is_connected(void) {
    // Check connection status
}
```

### Testing Plan

```bash
# Start local RabbitMQ
docker run -d --name rabbitmq -p 5672:5672 -p 15672:15672 rabbitmq:management

# Basic test
./tests/bin/rabbitmq_test_client '{"command":"status"}'

# Verify in management UI
# http://localhost:15672 (guest/guest)
```

---

## 🚧 Driver Redis (Roadmap)

### Objective

Support for Redis as messaging backend, allowing:
- Simple and fast Pub/Sub
- Response caching
- Rate limiting
- Low latency (<0.5ms)

### Planned Features

- 🔲 **Pub/Sub**: Native Redis Pub/Sub
- 🔲 **Streams**: Redis Streams for persistence
- 🔲 **List-based**: LPUSH/BRPOP for queues
- 🔲 **Caching**: GET/SET for responses
- 🔲 **Rate Limiting**: INCR/EXPIRE for throttling
- 🔲 **Sentinel**: High availability
- 🔲 **Cluster**: Horizontal sharding

### Proposed Configuration

```xml
<param name="driver" value="redis"/>
<param name="url" value="redis://localhost:6379"/>
<param name="node-id" value="fs-node-01"/>
<param name="redis-db" value="0"/>
<param name="redis-password" value=""/>
<param name="redis-mode" value="pubsub"/>  <!-- pubsub|streams|list -->
<param name="redis-channel-commands" value="freeswitch:commands"/>
<param name="redis-channel-events" value="freeswitch:events"/>
```

### Required Dependencies

- **hiredis**: Redis C client
- **Version**: >=1.0.0
- **Installation**: `apt install libhiredis-dev`

### Driver Interface

```c
// driver_redis.c - Methods to implement

switch_status_t redis_init(const char *url, const char *node_id) {
    // 1. Connect to Redis
    // 2. Authenticate if password
    // 3. SELECT database
    // 4. SUBSCRIBE to channels
    // 5. Start read thread
}

switch_status_t redis_subscribe_commands(command_callback_t callback) {
    // 1. Loop of redisGetReply
    // 2. Parse message
    // 3. Invoke callback
}

switch_status_t redis_send_command_response(const char *reply_subject, 
                                            const char *json_response) {
    // 1. PUBLISH to response channel
    // 2. Or SET with TTL for cache
}

switch_status_t redis_publish_event(const char *subject, const char *json_payload) {
    // 1. PUBLISH to event channel
    // 2. Or XADD to stream
}

void redis_shutdown(void) {
    // 1. UNSUBSCRIBE
    // 2. QUIT
    // 3. redisFree
}

switch_bool_t redis_is_connected(void) {
    // PING command
}
```

### Testing Plan

```bash
# Start local Redis
docker run -d --name redis -p 6379:6379 redis:alpine

# Basic test
./tests/bin/redis_test_client '{"command":"status"}'

# Real-time monitor
redis-cli MONITOR
```

---

## 🛠️ Implementation Guide

### Steps to Develop a New Driver

1. **Initial Setup**
   ```bash
   cd src/drivers
   cp driver_nats.c driver_mydriver.c
   ```

2. **Implement Interface**
   - Complete all methods of `event_driver_t`
   - See `driver_interface.h` for reference

3. **Add to Makefile**
   ```makefile
   ifdef WITH_MYDRIVER
   DRIVER_SRC += src/drivers/driver_mydriver.c
   DRIVER_LIBS += -lmydriverlib
   endif
   ```

4. **Create Tests**
   ```bash
   cd tests
   cp service_a_nats.c service_a_mydriver.c
   # Modify connection and subjects
   ```

5. **Documentation**
   - Update this ROADMAP
   - Add examples in `examples/`
   - Document XML configuration

6. **Submit PR**
   - Tests passing
   - Complete documentation
   - Updated changelog

---

## 📊 Prioritization

### High Priority
1. **Kafka** - Enterprise demand, massive event streaming
2. **RabbitMQ** - Mature ecosystem, many users

### Medium Priority
3. **Redis** - Simple, fast, good for MVP

### Low Priority
4. **AWS SQS** - Cloud-specific
5. **Google Pub/Sub** - Cloud-specific
6. **Azure Service Bus** - Cloud-specific

---

## 🤝 How to Contribute

Interested in implementing a driver?

1. **Discussion**: Open an issue to discuss the design
2. **Fork**: Fork the repository
3. **Branch**: `git checkout -b feature/driver-kafka`
4. **Implementation**: Follow the guide above
5. **Tests**: Ensure 100% coverage
6. **PR**: Create Pull Request with detailed description

---

## 📞 Contact

- **Issues**: https://github.com/zenozaga/freesweetch-agent-nats/issues
- **Discussions**: For questions about implementation

---

**Last updated**: December 2025
