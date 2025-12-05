# ROADMAP - mod_event_agent Drivers

Roadmap de desarrollo de drivers para `mod_event_agent`.

---

## 📋 Estado Actual

| Driver | Estado | Versión | Características | Notas |
|--------|--------|---------|----------------|-------|
| **NATS** | ✅ **Completo** | 1.0 | Request-Reply, Connection pooling, Auto-reconnect | Production-ready |
| **Kafka** | 🚧 Stub | 0.1 | Interface definida | Requiere implementación |
| **RabbitMQ** | 🚧 Stub | 0.1 | Interface definida | Requiere implementación |
| **Redis** | 🚧 Stub | 0.1 | Interface definida | Requiere implementación |

---

## ✅ Driver NATS (Completo)

### Características Implementadas

- ✅ **Conexión**: Inicialización con URL configurable
- ✅ **Request-Reply**: Comandos síncronos con respuestas
- ✅ **Fire-and-Forget**: Comandos asíncronos sin respuesta
- ✅ **Auto-Reconnect**: Reconexión automática con backoff exponencial
- ✅ **Health Check**: Verificación de estado de conexión
- ✅ **Node ID**: Identificación en clusters multi-nodo
- ✅ **JSON Serialization**: Payloads estructurados
- ✅ **Error Handling**: Manejo robusto de errores
- ✅ **Statistics**: Contadores de requests/successes/failures
- ✅ **Performance**: ~10,000 req/s, <1ms latencia local

### Configuración

```xml
<param name="driver" value="nats"/>
<param name="url" value="nats://localhost:4222"/>
<param name="node-id" value="fs-node-01"/>
<param name="nats-timeout" value="5000"/>
<param name="nats-max-reconnect" value="60"/>
<param name="nats-reconnect-wait" value="2000"/>
```

### Dependencias

- **NATS C Client**: v3.8.2
- **Biblioteca**: `lib/nats/libnats.so` (shared) o `lib/nats/libnats_static.a` (static)
- **Headers**: Incluidos en el proyecto

### Testing

```bash
# Compilar tests
cd tests && make

# Test básico
LD_LIBRARY_PATH=../lib/nats ./bin/service_a_nats '{"command":"status"}'

# Test de stress
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

### Dependencias Requeridas

- **librdkafka**: C/C++ Apache Kafka client
- **Versión**: >=1.9.0
- **Instalación**: `apt install librdkafka-dev` o compilar desde source

### Interface del Driver

```c
// driver_kafka.c - Métodos a implementar

switch_status_t kafka_init(const char *brokers, const char *node_id) {
    // 1. Crear configuración de Kafka
    // 2. Inicializar producer
    // 3. Inicializar consumer
    // 4. Suscribirse a topic de comandos
    // 5. Iniciar thread de polling
}

switch_status_t kafka_subscribe_commands(command_callback_t callback) {
    // 1. Consumer poll loop
    // 2. Deserializar mensaje
    // 3. Invocar callback con JSON
}

switch_status_t kafka_send_command_response(const char *reply_subject, 
                                            const char *json_response) {
    // 1. Determinar partition key (node_id)
    // 2. Producir mensaje a topic de respuestas
    // 3. Flush si batch completo
}

switch_status_t kafka_publish_event(const char *subject, const char *json_payload) {
    // 1. Mapear subject a topic
    // 2. Producir evento
}

void kafka_shutdown(void) {
    // 1. Flush pending messages
    // 2. Destruir producer
    // 3. Destruir consumer
    // 4. Cleanup threads
}

switch_bool_t kafka_is_connected(void) {
    // Verificar estado de producer y consumer
}
```

### Testing Plan

```bash
# Levantar Kafka local
docker run -d --name kafka -p 9092:9092 apache/kafka:latest

# Test básico
./tests/bin/kafka_test_client '{"command":"status"}'

# Test de throughput
./tests/bin/kafka_stress_test --messages 10000 --concurrency 50
```

### Contribución

Para implementar el driver Kafka:
1. Fork del repositorio
2. Copiar `src/drivers/driver_nats.c` → `src/drivers/driver_kafka.c`
3. Implementar funciones según interface arriba
4. Agregar `WITH_KAFKA=yes` en Makefile
5. Crear tests en `tests/kafka_test_client.c`
6. Documentar en este ROADMAP
7. Submit PR

---

## 🚧 Driver RabbitMQ (Roadmap)

### Objetivo

Soporte para RabbitMQ como backend de mensajería, permitiendo:
- Enterprise message queuing
- Routing complejo (exchanges, bindings)
- Garantías de entrega (acks, confirms)
- Integración con sistemas existentes

### Características Planeadas

- 🔲 **Publisher**: Publicación a exchanges
- 🔲 **Consumer**: Consumo desde queues
- 🔲 **Routing**: Topic exchanges con routing keys
- 🔲 **Acknowledgments**: Manual/automatic acks
- 🔲 **Publisher Confirms**: Confirmación de entrega
- 🔲 **Prefetch**: Control de flujo
- 🔲 **Dead Letter**: Manejo de mensajes fallidos

### Configuración Propuesta

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

### Dependencias Requeridas

- **librabbitmq**: RabbitMQ C client (rabbitmq-c)
- **Versión**: >=0.11.0
- **Instalación**: `apt install librabbitmq-dev`

### Interface del Driver

```c
// driver_rabbitmq.c - Métodos a implementar

switch_status_t rabbitmq_init(const char *url, const char *node_id) {
    // 1. Conectar a RabbitMQ
    // 2. Abrir channel
    // 3. Declarar exchange
    // 4. Declarar queue
    // 5. Bind queue a exchange
    // 6. Iniciar consumer
}

switch_status_t rabbitmq_subscribe_commands(command_callback_t callback) {
    // 1. Basic.Consume en queue
    // 2. Loop de recepción
    // 3. Invocar callback
    // 4. Basic.Ack
}

switch_status_t rabbitmq_send_command_response(const char *reply_subject, 
                                                const char *json_response) {
    // 1. Basic.Publish con reply-to y correlation-id
    // 2. Publisher confirm
}

switch_status_t rabbitmq_publish_event(const char *subject, const char *json_payload) {
    // 1. Mapear subject a routing key
    // 2. Basic.Publish a exchange
}

void rabbitmq_shutdown(void) {
    // 1. Cancelar consumer
    // 2. Cerrar channel
    // 3. Cerrar conexión
}

switch_bool_t rabbitmq_is_connected(void) {
    // Verificar estado de conexión
}
```

### Testing Plan

```bash
# Levantar RabbitMQ local
docker run -d --name rabbitmq -p 5672:5672 -p 15672:15672 rabbitmq:management

# Test básico
./tests/bin/rabbitmq_test_client '{"command":"status"}'

# Verificar en management UI
# http://localhost:15672 (guest/guest)
```

---

## 🚧 Driver Redis (Roadmap)

### Objetivo

Soporte para Redis como backend de mensajería, permitiendo:
- Pub/Sub simple y rápido
- Caching de respuestas
- Rate limiting
- Baja latencia (<0.5ms)

### Características Planeadas

- 🔲 **Pub/Sub**: Redis Pub/Sub nativo
- 🔲 **Streams**: Redis Streams para persistencia
- 🔲 **List-based**: LPUSH/BRPOP para queues
- 🔲 **Caching**: GET/SET para responses
- 🔲 **Rate Limiting**: INCR/EXPIRE para throttling
- 🔲 **Sentinel**: Alta disponibilidad
- 🔲 **Cluster**: Sharding horizontal

### Configuración Propuesta

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

### Dependencias Requeridas

- **hiredis**: Redis C client
- **Versión**: >=1.0.0
- **Instalación**: `apt install libhiredis-dev`

### Interface del Driver

```c
// driver_redis.c - Métodos a implementar

switch_status_t redis_init(const char *url, const char *node_id) {
    // 1. Conectar a Redis
    // 2. Autenticar si password
    // 3. SELECT database
    // 4. SUBSCRIBE a canales
    // 5. Iniciar thread de lectura
}

switch_status_t redis_subscribe_commands(command_callback_t callback) {
    // 1. Loop de redisGetReply
    // 2. Parse mensaje
    // 3. Invocar callback
}

switch_status_t redis_send_command_response(const char *reply_subject, 
                                            const char *json_response) {
    // 1. PUBLISH a canal de respuesta
    // 2. O SET con TTL para cache
}

switch_status_t redis_publish_event(const char *subject, const char *json_payload) {
    // 1. PUBLISH a canal de eventos
    // 2. O XADD a stream
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
# Levantar Redis local
docker run -d --name redis -p 6379:6379 redis:alpine

# Test básico
./tests/bin/redis_test_client '{"command":"status"}'

# Monitor en tiempo real
redis-cli MONITOR
```

---

## 🛠️ Guía de Implementación

### Pasos para Desarrollar un Nuevo Driver

1. **Setup Inicial**
   ```bash
   cd src/drivers
   cp driver_nats.c driver_mydriver.c
   ```

2. **Implementar Interface**
   - Completar todos los métodos de `event_driver_t`
   - Ver `driver_interface.h` para referencia

3. **Agregar a Makefile**
   ```makefile
   ifdef WITH_MYDRIVER
   DRIVER_SRC += src/drivers/driver_mydriver.c
   DRIVER_LIBS += -lmydriverlib
   endif
   ```

4. **Crear Tests**
   ```bash
   cd tests
   cp service_a_nats.c service_a_mydriver.c
   # Modificar conexión y subjects
   ```

5. **Documentación**
   - Actualizar este ROADMAP
   - Agregar ejemplos en `examples/`
   - Documentar configuración XML

6. **Submit PR**
   - Tests pasando
   - Documentación completa
   - Changelog actualizado

---

## 📊 Priorización

### Alta Prioridad
1. **Kafka** - Demanda enterprise, event streaming masivo
2. **RabbitMQ** - Ecosistema maduro, muchos usuarios

### Media Prioridad
3. **Redis** - Simple, rápido, bueno para MVP

### Baja Prioridad
4. **AWS SQS** - Cloud-specific
5. **Google Pub/Sub** - Cloud-specific
6. **Azure Service Bus** - Cloud-specific

---

## 🤝 Cómo Contribuir

¿Interesado en implementar un driver?

1. **Discusión**: Abre un issue para discutir el diseño
2. **Fork**: Fork del repositorio
3. **Branch**: `git checkout -b feature/driver-kafka`
4. **Implementación**: Sigue la guía arriba
5. **Tests**: Asegura 100% de cobertura
6. **PR**: Crea Pull Request con descripción detallada

---

## 📞 Contacto

- **Issues**: https://github.com/zenozaga/freesweetch-agent-nats/issues
- **Discussions**: Para preguntas sobre implementación

---

**Última actualización**: Diciembre 2025
