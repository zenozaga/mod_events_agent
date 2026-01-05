# ROADMAP - mod_event_agent

Development roadmap and feature planning for `mod_event_agent`.

---

## 📊 Current Status (v2.0.0)

### ✅ Production Features

| Component | Status | Version | Description |
|-----------|--------|---------|-------------|
| **NATS Driver** | ✅ Complete | 2.0 | Production-ready with all features |
| **Command System** | ✅ Complete | 2.0 | Generic API + structured commands |
| **Event Streaming** | ✅ Complete | 2.0 | Real-time event pub/sub |
| **Dialplan Manager** | ✅ Complete | 2.0 | Dynamic park mode with audio control |
| **Multi-Node Support** | ✅ Complete | 2.0 | Broadcast + direct routing |
| **Call Control** | ✅ Complete | 2.0 | Originate + bridge operations |

### 🚧 Roadmap Features

| Feature | Status | Priority | Target |
|---------|--------|----------|--------|
| **Kafka Driver** | 📋 Planned | Medium | v3.0 |
| **RabbitMQ Driver** | 📋 Planned | Low | v3.0 |
| **Redis Driver** | 📋 Planned | Low | v3.0 |
| **WebSocket Driver** | 💡 Idea | Low | Future |
| **gRPC Driver** | 💡 Idea | Low | Future |

---

## ✅ NATS Driver (Production-Ready)

### Implemented Features

#### Core Functionality
- ✅ **Connection Management**: URL-based initialization with health checks
- ✅ **Auto-Reconnect**: Exponential backoff with configurable retries
- ✅ **Request-Reply**: Synchronous command execution with responses
- ✅ **Pub/Sub**: Asynchronous event streaming
- ✅ **Error Handling**: Comprehensive error reporting and recovery
- ✅ **Statistics**: Real-time metrics (sent, failed, bytes, reconnects)

#### Advanced Features
- ✅ **Broadcast Subscriptions**: All-node delivery with JSON filtering
- ✅ **Direct Subscriptions**: Per-node routing via subject hierarchy
- ✅ **Node Identification**: Slugified node IDs for NATS subjects
- ✅ **Connection Callbacks**: Reconnect/disconnect event handlers
- ✅ **Thread Safety**: Mutex-protected operations
- ✅ **Memory Management**: Proper cleanup on shutdown

### Performance Characteristics

| Metric | Value | Notes |
|--------|-------|-------|
| **Throughput** | 10,000+ req/sec | Local NATS server |
| **Latency (p50)** | <0.5ms | Request-reply roundtrip |
| **Latency (p99)** | <2ms | Request-reply roundtrip |
| **CPU Overhead** | <0.1% | Per request |
| **Memory** | ~5MB | Baseline footprint |
| **Reconnect Time** | 2s (configurable) | Auto-recovery |

### Configuration Options

```xml
<configuration name="event_agent.conf">
  <settings>
    <!-- Connection -->
    <param name="driver" value="nats"/>
    <param name="url" value="nats://localhost:4222"/>
    
    <!-- Identity -->
    <param name="node-id" value="fs-node-01"/>
    <param name="subject-prefix" value="fs"/>
    
    <!-- NATS-specific -->
    <param name="nats-timeout" value="5000"/>           <!-- Request timeout (ms) -->
    <param name="nats-max-reconnect" value="60"/>       <!-- Max reconnect attempts -->
    <param name="nats-reconnect-wait" value="2000"/>    <!-- Reconnect delay (ms) -->
    
    <!-- Authentication (optional) -->
    <param name="token" value="secret"/>                <!-- Token auth -->
    <!-- OR -->
    <param name="nkey_seed" value="SUAXXXXX"/>         <!-- NKey auth -->
    
    <!-- Event filtering -->
    <param name="include-events" value="CHANNEL_CREATE,CHANNEL_ANSWER"/>
    <param name="exclude-events" value="HEARTBEAT,PRESENCE_IN"/>
  </settings>
</configuration>
```

### Security Features

- ✅ **Token Authentication**: Simple bearer token
- ✅ **NKey Authentication**: Ed25519 public-key cryptography
- ✅ **TLS Support**: Encrypted transport (NATS server config)
- ✅ **Subject ACLs**: Permission-based access control (NATS server)

### Monitoring & Observability

**Available Metrics** (via `freeswitch.cmd.status`):
```json
{
  "driver": {
    "name": "nats",
    "connected": true,
    "stats": {
      "messages_sent": 12345,
      "messages_failed": 2,
      "bytes_sent": 1048576,
      "reconnects": 1
    }
  }
}
```

**Health Check**:
- Connection state monitoring
- Automatic reconnection on failure
- Exponential backoff on repeated failures

---

## 📋 Planned Drivers

### 🚧 Kafka Driver (v3.0)

**Target Use Cases**:
- High-throughput event streaming (>100k events/sec)
- Event replay and retention
- Multi-consumer patterns
- Integration with Apache ecosystem

**Planned Features**:
- ✅ Event publishing to Kafka topics
- ✅ Partitioning by node_id or call_uuid
- ✅ Configurable retention policies
- ⚠️ Command support (request-reply pattern TBD)
- ⚠️ Consumer group management

**Technical Requirements**:
- Library: `librdkafka`
- Protocol: Apache Kafka binary protocol
- Topics: `freeswitch.events.*`, `freeswitch.commands.*`

**Configuration Sketch**:
```xml
<param name="driver" value="kafka"/>
<param name="brokers" value="localhost:9092"/>
<param name="topic-prefix" value="freeswitch"/>
<param name="partition-key" value="node_id"/>
```

**Challenges**:
- Request-reply pattern (Kafka is primarily log-based)
- Need separate request/response topics
- Higher latency vs NATS (~5-10ms)

---

### 🚧 RabbitMQ Driver (v3.0)

**Target Use Cases**:
- Enterprise messaging with AMQP protocol
- Complex routing rules
- Message persistence
- Dead letter queues

**Planned Features**:
- ✅ Request-reply via RPC pattern
- ✅ Event publishing to exchanges
- ✅ Durable queues for reliability
- ✅ Topic-based routing
- ⚠️ Cluster support

**Technical Requirements**:
- Library: `librabbitmq` or `amqp-cpp`
- Protocol: AMQP 0-9-1
- Exchanges: Topic exchange for events

**Configuration Sketch**:
```xml
<param name="driver" value="rabbitmq"/>
<param name="host" value="localhost"/>
<param name="port" value="5672"/>
<param name="vhost" value="/"/>
<param name="exchange" value="freeswitch"/>
```

---

### 🚧 Redis Driver (v3.0)

**Target Use Cases**:
- Low-latency caching + pub/sub
- Session state storage
- Real-time presence
- Simple deployments

**Planned Features**:
- ✅ Pub/sub for events
- ✅ Request-reply via BLPOP/RPUSH pattern
- ✅ Connection pooling
- ⚠️ Cluster mode support
- ⚠️ Sentinel support

**Technical Requirements**:
- Library: `hiredis`
- Protocol: RESP (Redis Serialization Protocol)
- Channels: `fs:events:*`, `fs:commands:*`

**Configuration Sketch**:
```xml
<param name="driver" value="redis"/>
<param name="host" value="localhost"/>
<param name="port" value="6379"/>
<param name="password" value="secret"/>
<param name="db" value="0"/>
```

---

## 💡 Future Ideas

### WebSocket Driver

**Use Case**: Direct browser/mobile integration

**Features**:
- WebSocket server embedded in FreeSWITCH
- JSON-RPC 2.0 protocol
- TLS/WSS support
- Authentication via JWT tokens

### gRPC Driver

**Use Case**: High-performance microservices

**Features**:
- Protocol Buffers for serialization
- Bidirectional streaming
- Load balancing
- Service mesh integration

### HTTP/REST Driver

**Use Case**: Simple HTTP clients

**Features**:
- RESTful API endpoint
- SSE (Server-Sent Events) for event streaming
- OpenAPI/Swagger documentation
- CORS support

---

## 📈 Version History

### v2.0.0 (Current)
- ✅ Complete NATS driver
- ✅ Dynamic dialplan manager
- ✅ Call control commands
- ✅ Multi-node support
- ✅ Event streaming
- ✅ Project restructure (modular)

### v1.0.0 (Legacy)
- ✅ Initial NATS implementation
- ✅ Generic API execution
- ✅ Basic event streaming

---

## 🎯 Development Priorities

### High Priority
1. ✅ **Documentation** - Complete API reference
2. ✅ **Testing** - Production validation
3. ✅ **Examples** - Python client samples

### Medium Priority
1. 📋 **Kafka Driver** - Implement core functionality
2. 📋 **Performance Testing** - Benchmark suite
3. 📋 **Monitoring** - Prometheus metrics exporter

### Low Priority
1. 📋 **RabbitMQ Driver** - AMQP support
2. 📋 **Redis Driver** - Lightweight option
3. 💡 **WebSocket Driver** - Browser integration

---

## 🤝 Contributing

Want to help implement a driver? See contribution guidelines:

1. Review driver interface: `src/drivers/interface.h`
2. Study NATS implementation: `src/drivers/nats.c`
3. Create stub: `src/drivers/{driver}.c`
4. Implement interface methods
5. Add configuration parsing
6. Write tests
7. Submit PR

**Contact**: Open an issue or PR on GitHub

---

## 📚 Resources

### NATS
- [NATS Documentation](https://docs.nats.io/)
- [NATS C Client](https://github.com/nats-io/nats.c)

### Kafka
- [Apache Kafka](https://kafka.apache.org/)
- [librdkafka](https://github.com/confluentinc/librdkafka)

### RabbitMQ
- [RabbitMQ Documentation](https://www.rabbitmq.com/documentation.html)
- [librabbitmq](https://github.com/alanxz/rabbitmq-c)

### Redis
- [Redis Documentation](https://redis.io/documentation)
- [hiredis](https://github.com/redis/hiredis)

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
