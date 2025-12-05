# Examples - mod_event_agent

Ejemplos prácticos de clientes para interactuar con FreeSWITCH mediante `mod_event_agent`.

---

## 📋 Ejemplos Disponibles

| Ejemplo | Lenguaje | Descripción | Dificultad |
|---------|----------|-------------|------------|
| **nats_command_client** | C | Cliente básico request-reply | ⭐ Fácil |
| **nats_subscriber** | C | Subscriber de eventos (futuro) | ⭐ Fácil |
| **call_monitor** | C | Monitor de llamadas en tiempo real | ⭐⭐ Media |
| **Tests integrados** | C | Clientes de test completos | ⭐⭐ Media |

---

## 🚀 Quick Start

### Opción 1: Usar Clientes de Test (Recomendado)

Los clientes más completos están en `/tests`:

```bash
cd tests
make

# Cliente que envía comandos
LD_LIBRARY_PATH=../lib/nats ./bin/service_a_nats '{"command":"status"}'

# Cliente interactivo
LD_LIBRARY_PATH=../lib/nats ./bin/service_a_nats
> {"command":"version"}
> {"command":"show","args":"channels"}
> quit

# Cliente multi-modo
LD_LIBRARY_PATH=../lib/nats ./bin/simple_test req freeswitch.api '{"command":"status"}'
```

### Opción 2: Compilar Ejemplos

```bash
cd examples
make

# Cliente básico
./nats_command_client
```

---

## 📖 Ejemplos Detallados

### 1. nats_command_client.c

Cliente básico en C que demuestra el patrón request-reply.

**Características:**
- Conexión a NATS
- Envío de comandos JSON
- Recepción de respuestas
- Manejo de errores

**Código:**
```c
#include <nats/nats.h>
#include <stdio.h>

int main() {
    natsConnection *conn = NULL;
    natsMsg *reply = NULL;
    natsStatus s;
    
    // Conectar a NATS
    s = natsConnection_ConnectTo(&conn, "nats://localhost:4222");
    if (s != NATS_OK) {
        printf("Error connecting: %s\n", natsStatus_GetText(s));
        return 1;
    }
    
    // Enviar comando
    const char *request = "{\"command\":\"status\"}";
    s = natsConnection_RequestString(&reply, conn, 
                                     "freeswitch.api", 
                                     request, 
                                     5000);
    
    if (s == NATS_OK) {
        printf("Response: %s\n", natsMsg_GetData(reply));
        natsMsg_Destroy(reply);
    }
    
    natsConnection_Destroy(conn);
    return 0;
}
```

**Compilar:**
```bash
gcc -o nats_command_client nats_command_client.c \
    -I../lib/nats/include \
    -L../lib/nats -lnats \
    -Wl,-rpath,../lib/nats
```

**Ejecutar:**
```bash
./nats_command_client
```

---

### 2. nats_subscriber.c

Subscriber de eventos de FreeSWITCH (preparado para futura implementación).

**Características:**
- Suscripción a eventos
- Deserialización JSON
- Procesamiento en tiempo real

**Uso futuro:**
```bash
./nats_subscriber freeswitch.events.CHANNEL_CREATE
./nats_subscriber "freeswitch.events.*"
```

---

### 3. call_monitor.c

Monitor completo de llamadas en tiempo real.

**Características:**
- Dashboard de llamadas activas
- Estadísticas en tiempo real
- Alertas de eventos importantes

**Compilar:**
```bash
cd examples
make call_monitor
```

**Ejecutar:**
```bash
./call_monitor nats://localhost:4222
```

---

## 💡 Casos de Uso Prácticos

### Caso 1: CLI Simple

Crear un CLI para control rápido de FreeSWITCH:

```bash
#!/bin/bash
# fs-cli: CLI simple para FreeSWITCH

NATS_URL="nats://localhost:4222"
SUBJECT="freeswitch.api"

case "$1" in
  status)
    ../tests/bin/simple_test req $SUBJECT '{"command":"status"}'
    ;;
  channels)
    ../tests/bin/simple_test req $SUBJECT '{"command":"show","args":"channels"}'
    ;;
  calls)
    ../tests/bin/simple_test req $SUBJECT '{"command":"show","args":"calls"}'
    ;;
  *)
    echo "Usage: fs-cli {status|channels|calls}"
    ;;
esac
```

**Usar:**
```bash
chmod +x fs-cli
./fs-cli status
./fs-cli channels
```

---

### Caso 2: Monitor de Llamadas

Dashboard simple en Python:

```python
#!/usr/bin/env python3
import asyncio
from nats.aio.client import Client as NATS
import json
from datetime import datetime

async def monitor_calls():
    nc = NATS()
    await nc.connect("nats://localhost:4222")
    
    while True:
        # Obtener llamadas activas
        request = json.dumps({"command": "show", "args": "calls"})
        response = await nc.request("freeswitch.api", request.encode(), timeout=2)
        
        data = json.loads(response.data.decode())
        if data['success']:
            print(f"\n[{datetime.now()}] Llamadas Activas:")
            print(data['data'])
        
        await asyncio.sleep(5)  # Actualizar cada 5 segundos
    
    await nc.close()

if __name__ == '__main__':
    asyncio.run(monitor_calls())
```

---

### Caso 3: Webhook Processor

Procesar webhooks y ejecutar comandos en FreeSWITCH:

```javascript
// webhook-processor.js
const express = require('express');
const { connect, StringCodec } = require('nats');

const app = express();
app.use(express.json());

let nc;

async function init() {
    nc = await connect({ servers: 'nats://localhost:4222' });
}

app.post('/originate', async (req, res) => {
    const { endpoint, destination } = req.body;
    
    const sc = StringCodec();
    const request = JSON.stringify({
        command: 'originate',
        args: `${endpoint} ${destination}`
    });
    
    const response = await nc.request('freeswitch.api', sc.encode(request), { timeout: 5000 });
    const data = JSON.parse(sc.decode(response.data));
    
    res.json(data);
});

init().then(() => {
    app.listen(3000, () => console.log('Webhook server running on :3000'));
});
```

---

### Caso 4: Integración con CRM

Sincronizar llamadas con CRM:

```python
#!/usr/bin/env python3
import asyncio
from nats.aio.client import Client as NATS
import json
import requests

CRM_API = "https://crm.example.com/api/calls"

async def log_call_to_crm(call_data):
    """Enviar información de llamada al CRM"""
    response = requests.post(CRM_API, json=call_data)
    return response.status_code == 200

async def originate_from_crm(customer_phone, agent_extension):
    """Originar llamada desde CRM"""
    nc = NATS()
    await nc.connect("nats://localhost:4222")
    
    request = json.dumps({
        "command": "originate",
        "args": f"user/{agent_extension} {customer_phone}"
    })
    
    response = await nc.request("freeswitch.api", request.encode(), timeout=5)
    data = json.loads(response.data.decode())
    
    if data['success']:
        # Log en CRM
        await log_call_to_crm({
            'agent': agent_extension,
            'customer': customer_phone,
            'uuid': data['data'],
            'status': 'initiated'
        })
    
    await nc.close()
    return data

# Uso desde CRM webhook:
# POST /api/originate {"customer_phone":"5551234567","agent_extension":"1000"}
```

---

## 🔧 Compilación de Ejemplos

### Makefile para Ejemplos

```makefile
# examples/Makefile

CC = gcc
CFLAGS = -Wall -I../lib/nats/include
LDFLAGS = -L../lib/nats -lnats -lpthread
RPATH = -Wl,-rpath,../lib/nats

EXAMPLES = nats_command_client call_monitor nats_subscriber

all: $(EXAMPLES)

nats_command_client: nats_command_client.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(RPATH)

call_monitor: call_monitor.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(RPATH)

nats_subscriber: nats_subscriber.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(RPATH)

clean:
	rm -f $(EXAMPLES)

.PHONY: all clean
```

**Compilar:**
```bash
cd examples
make
```

---

## 📚 Referencias

- **[docs/API.md](../docs/API.md)**: Documentación completa de la API
- **[tests/](../tests/)**: Clientes de test más completos
- **[README.md](../README.md)**: Documentación principal del módulo

---

## 🐛 Troubleshooting

### Error: "Cannot connect to NATS"

```bash
# Verificar que NATS esté corriendo
docker ps | grep nats

# O verificar puerto
netstat -an | grep 4222
```

### Error: "Shared library not found"

```bash
# Agregar al LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/path/to/lib/nats:$LD_LIBRARY_PATH

# O usar RPATH en compilación
gcc ... -Wl,-rpath,/path/to/lib/nats
```

### Error: "No response from FreeSWITCH"

```bash
# Verificar que mod_event_agent esté cargado
docker exec -it freeswitch fs_cli -x "module_exists mod_event_agent"

# Ver logs
docker logs freeswitch | grep event_agent
```

---

**Última actualización**: Diciembre 2025

---

## Development

### Adding New Commands

Edit `nats_command_client.c` and add new command handlers following the pattern:

```c
if (strcmp(command, "your_command") == 0) {
    // Build request JSON
    cJSON *request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "command", "your_api_command");
    
    // Send via NATS
    send_nats_request(nc, "freeswitch.api", request);
}
```

---

## Troubleshooting

### Connection Issues

```bash
# Check NATS server is running
docker ps | grep nats

# Check port mapping
netstat -an | grep 5800

# Test NATS connectivity
nats-server --version
```

### Module Issues

```bash
# Check module is loaded
docker exec agent_nats_dev_freeswitch fs_cli -x "module_exists mod_event_agent"

# Check NATS subscriptions
nats sub "freeswitch.>" --count=10
```

---

#### d) Execute - Execute Application
```bash
./nats_command_client execute <uuid> <app> [args]
```

**Examples:**
```bash
# Play audio
./nats_command_client execute d0c6f8b4-1234-5678-90ab-cdef12345678 playback /tmp/hello.wav

# TTS
./nats_command_client execute d0c6f8b4-1234-5678-90ab-cdef12345678 speak "Hello World"

# Record
./nats_command_client execute d0c6f8b4-1234-5678-90ab-cdef12345678 record /tmp/recording.wav
```

## Environment Variables

```bash
export NATS_URL="nats://192.168.1.100:4222"
./nats_subscriber
```

## Troubleshooting

### Error: "Failed to connect to NATS"
- Verify NATS server is running: `nats-server`
- Check URL: `NATS_URL=nats://localhost:4222`

### Error: "Request failed"
- Verify FreeSWITCH has mod_event_agent loaded
- Check logs: `grep mod_event_agent /var/log/freeswitch/freeswitch.log`

### No events received
- Verify mod_event_agent is publishing: `nats sub "freeswitch.events.>"`
- Make a test call

## Subject Patterns

### Events (Pub/Sub):
- `freeswitch.events.>` - All events
- `freeswitch.events.channel.create` - New channels
- `freeswitch.events.channel.answer` - Answered calls
- `freeswitch.events.channel.hangup` - Ended calls

### Commands (Request/Reply):
- `freeswitch.cmd.status` - System status
- `freeswitch.cmd.originate` - Originate calls
- `freeswitch.cmd.hangup` - Hang up channels
- `freeswitch.cmd.execute` - Execute applications

## References

- [NATS C Client](https://github.com/nats-io/nats.c)
- [FreeSWITCH Docs](https://freeswitch.org/confluence/)
- [mod_event_agent README](../README.md)
