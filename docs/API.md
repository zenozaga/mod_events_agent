# API Reference - mod_event_agent

Documentación completa de la API para control de FreeSWITCH mediante `mod_event_agent`.

---

## 📋 Tabla de Contenidos

- [Arquitectura de Comunicación](#arquitectura-de-comunicación)
- [Formato de Mensajes](#formato-de-mensajes)
- [Comandos API](#comandos-api)
- [Códigos de Respuesta](#códigos-de-respuesta)
- [Ejemplos de Uso](#ejemplos-de-uso)
- [Manejo de Errores](#manejo-de-errores)

---

## 🏗️ Arquitectura de Comunicación

### Patrón Request-Reply

```
┌──────────┐                      ┌──────────┐                    ┌────────────┐
│  Client  │                      │   NATS   │                    │ FreeSWITCH │
│ Service  │                      │  Broker  │                    │ mod_event  │
└────┬─────┘                      └────┬─────┘                    └─────┬──────┘
     │                                 │                                │
     │ 1. PUB freeswitch.api          │                                │
     │    Reply: _INBOX.12345          │                                │
     ├────────────────────────────────>│                                │
     │                                 │ 2. Deliver message             │
     │                                 ├───────────────────────────────>│
     │                                 │                                │
     │                                 │ 3. Execute API command         │
     │                                 │<───────────────────────────────┤
     │                                 │                                │
     │ 4. Response on _INBOX.12345     │                                │
     │<────────────────────────────────┤                                │
     │                                 │                                │
     └─────────────────────────────────┴────────────────────────────────┘
```

### Subjects (Tópicos) Disponibles

| Subject | Tipo | Descripción |
|---------|------|-------------|
| `freeswitch.api` | Request-Reply | Comandos síncronos con respuesta |
| `freeswitch.cmd.async.*` | Fire-and-Forget | Comandos asíncronos sin respuesta |
| `freeswitch.events.*` | Pub/Sub | Eventos de FreeSWITCH (futuro) |

---

## 📦 Formato de Mensajes

### Request (Cliente → FreeSWITCH)

```json
{
  "command": "string",      // Comando API de FreeSWITCH (requerido)
  "args": "string",         // Argumentos del comando (opcional)
  "node_id": "string"       // Target node ID para clusters (opcional)
}
```

#### Campos

- **`command`** (string, requerido): Nombre del comando API de FreeSWITCH
- **`args`** (string, opcional): Argumentos adicionales para el comando
- **`node_id`** (string, opcional): ID del nodo específico en clusters multi-nodo

### Response (FreeSWITCH → Cliente)

```json
{
  "success": boolean,       // true si comando ejecutó correctamente
  "message": "string",      // Mensaje descriptivo del resultado
  "data": "string",         // Respuesta del comando (puede ser null)
  "timestamp": number,      // Unix timestamp en microsegundos
  "node_id": "string"       // ID del nodo que procesó el comando
}
```

#### Campos

- **`success`** (boolean): Indica si el comando se ejecutó sin errores
- **`message`** (string): Descripción del resultado o error
- **`data`** (string): Salida del comando FreeSWITCH (formato depende del comando)
- **`timestamp`** (number): Timestamp Unix en microsegundos
- **`node_id`** (string): Identificador del nodo FreeSWITCH que procesó

---

## 🎯 Comandos API

### Comandos de Sistema

#### `status`
Obtiene el estado del sistema FreeSWITCH.

**Request:**
```json
{"command":"status"}
```

**Response:**
```json
{
  "success": true,
  "message": "API command executed",
  "data": "UP 0 years, 0 days, 0 hours, 3 minutes, 45 seconds, 678 milliseconds, 901 microseconds\nFreeSWITCH (Version 1.10.10-release ...) is ready\n0 session(s) since startup\n0 session(s) - peak 0, last 5min 0\n0 session(s) per Sec out of max 30, peak 0, last 5min 0\n1000 session(s) max\nmin idle cpu 0.00/100.00",
  "timestamp": 1764893599366416,
  "node_id": "fs-node-01"
}
```

#### `version`
Obtiene la versión de FreeSWITCH.

**Request:**
```json
{"command":"version"}
```

**Response:**
```json
{
  "success": true,
  "message": "API command executed",
  "data": "FreeSWITCH Version 1.10.10-release+git~20230813T165739Z~d506bc6c3c~64bit (git d506bc6 2023-08-13 16:57:39Z 64bit)",
  "timestamp": 1764893545123456,
  "node_id": "fs-node-01"
}
```

#### `uptime`
Obtiene el tiempo de actividad del sistema.

**Request:**
```json
{"command":"uptime"}
```

**Response:**
```json
{
  "success": true,
  "message": "API command executed",
  "data": "0 years, 0 days, 1 hours, 23 minutes, 45 seconds, 678 milliseconds, 901 microseconds",
  "timestamp": 1764893600000000,
  "node_id": "fs-node-01"
}
```

---

### Comandos de Variables

#### `global_getvar`
Obtiene el valor de una variable global.

**Request:**
```json
{
  "command": "global_getvar",
  "args": "hostname"
}
```

**Response:**
```json
{
  "success": true,
  "message": "API command executed",
  "data": "e8e1491c7b69",
  "timestamp": 1764893599366416,
  "node_id": "fs-node-01"
}
```

#### `global_setvar`
Establece una variable global.

**Request:**
```json
{
  "command": "global_setvar",
  "args": "my_var=my_value"
}
```

**Response:**
```json
{
  "success": true,
  "message": "API command executed",
  "data": "+OK",
  "timestamp": 1764893601000000,
  "node_id": "fs-node-01"
}
```

---

### Comandos de Información

#### `show modules`
Lista todos los módulos cargados.

**Request:**
```json
{
  "command": "show",
  "args": "modules"
}
```

**Response:**
```json
{
  "success": true,
  "message": "API command executed",
  "data": "type,name,ikey,filename\napi,...,mod_commands,/usr/local/freeswitch/mod/mod_commands.so\n...\n517 total.",
  "timestamp": 1764893612515194,
  "node_id": "fs-node-01"
}
```

#### `show channels`
Lista todos los canales activos.

**Request:**
```json
{
  "command": "show",
  "args": "channels"
}
```

**Response:**
```json
{
  "success": true,
  "message": "API command executed",
  "data": "uuid,direction,created,created_epoch,name,state,cid_name,cid_num...\n0 total.",
  "timestamp": 1764893650000000,
  "node_id": "fs-node-01"
}
```

#### `show calls`
Lista todas las llamadas activas.

**Request:**
```json
{
  "command": "show",
  "args": "calls"
}
```

**Response:**
```json
{
  "success": true,
  "message": "API command executed",
  "data": "uuid,direction,created,created_epoch,name,state,cid_name,cid_num...\n0 total.",
  "timestamp": 1764893660000000,
  "node_id": "fs-node-01"
}
```

---

### Comandos SIP (Sofia)

#### `sofia status`
Obtiene el estado de todos los perfiles SIP.

**Request:**
```json
{
  "command": "sofia",
  "args": "status"
}
```

**Response:**
```json
{
  "success": true,
  "message": "API command executed",
  "data": "                     Name\t    Type\t                                      Data\tState\n======================================================================================\n             drachtio_mrf\tprofile\t             sip:mod_sofia@172.18.0.3:5080\tRUNNING (0)\n======================================================================================\n1 profile 0 aliases\n",
  "timestamp": 1764893699325070,
  "node_id": "fs-node-01"
}
```

#### `sofia status profile <name>`
Obtiene el estado de un perfil SIP específico.

**Request:**
```json
{
  "command": "sofia",
  "args": "status profile drachtio_mrf"
}
```

---

### Comandos de Llamadas

#### `originate`
Origina una nueva llamada.

**Request:**
```json
{
  "command": "originate",
  "args": "user/1000 &park()"
}
```

**Response:**
```json
{
  "success": true,
  "message": "API command executed",
  "data": "+OK 550e8400-e29b-41d4-a716-446655440000",
  "timestamp": 1764893700000000,
  "node_id": "fs-node-01"
}
```

#### `uuid_kill`
Finaliza una llamada por UUID.

**Request:**
```json
{
  "command": "uuid_kill",
  "args": "550e8400-e29b-41d4-a716-446655440000"
}
```

**Response:**
```json
{
  "success": true,
  "message": "API command executed",
  "data": "+OK",
  "timestamp": 1764893710000000,
  "node_id": "fs-node-01"
}
```

#### `hupall`
Finaliza todas las llamadas activas.

**Request:**
```json
{
  "command": "hupall",
  "args": "NORMAL_CLEARING"
}
```

**Response:**
```json
{
  "success": true,
  "message": "API command executed",
  "data": "+OK 15 calls hung up",
  "timestamp": 1764893720000000,
  "node_id": "fs-node-01"
}
```

---

### Comandos de Módulos

#### `load`
Carga un módulo dinámicamente.

**Request:**
```json
{
  "command": "load",
  "args": "mod_conference"
}
```

**Response:**
```json
{
  "success": true,
  "message": "API command executed",
  "data": "+OK",
  "timestamp": 1764893730000000,
  "node_id": "fs-node-01"
}
```

#### `unload`
Descarga un módulo.

**Request:**
```json
{
  "command": "unload",
  "args": "mod_conference"
}
```

**Response:**
```json
{
  "success": true,
  "message": "API command executed",
  "data": "+OK",
  "timestamp": 1764893740000000,
  "node_id": "fs-node-01"
}
```

#### `reload`
Recarga un módulo.

**Request:**
```json
{
  "command": "reload",
  "args": "mod_event_agent"
}
```

**Response:**
```json
{
  "success": true,
  "message": "API command executed",
  "data": "+OK",
  "timestamp": 1764893750000000,
  "node_id": "fs-node-01"
}
```

---

## 📊 Códigos de Respuesta

### Success States

| Estado | `success` | Descripción |
|--------|-----------|-------------|
| Comando ejecutado | `true` | El comando se ejecutó correctamente |
| Comando sin salida | `true` | Comando exitoso pero sin datos de retorno (`data: null`) |

### Error States

| Estado | `success` | `message` | Descripción |
|--------|-----------|-----------|-------------|
| JSON inválido | `false` | `"Invalid JSON format"` | El payload no es JSON válido |
| Campo faltante | `false` | `"Missing 'command' field"` | Falta el campo `command` en el JSON |
| Comando inválido | `false` | `"API command failed"` | El comando no existe o falló su ejecución |
| Error interno | `false` | `"Internal error"` | Error interno del módulo o FreeSWITCH |

---

## 💡 Ejemplos de Uso

### Ejemplo 1: Cliente Básico en C

```c
#include <stdio.h>
#include <nats/nats.h>

int main() {
    natsConnection *conn = NULL;
    natsMsg *reply = NULL;
    
    // Conectar a NATS
    natsConnection_ConnectTo(&conn, "nats://localhost:4222");
    
    // Enviar comando
    const char *request = "{\"command\":\"status\"}";
    natsConnection_RequestString(&reply, conn, "freeswitch.api", request, 5000);
    
    // Procesar respuesta
    printf("Response: %s\n", natsMsg_GetData(reply));
    
    // Cleanup
    natsMsg_Destroy(reply);
    natsConnection_Destroy(conn);
    return 0;
}
```

### Ejemplo 2: Cliente en Python

```python
import asyncio
from nats.aio.client import Client as NATS
import json

async def main():
    nc = NATS()
    await nc.connect("nats://localhost:4222")
    
    # Enviar comando
    request = json.dumps({"command": "status"})
    response = await nc.request("freeswitch.api", request.encode(), timeout=5)
    
    # Procesar respuesta
    data = json.loads(response.data.decode())
    print(f"Success: {data['success']}")
    print(f"Data: {data['data']}")
    
    await nc.close()

if __name__ == '__main__':
    asyncio.run(main())
```

### Ejemplo 3: Cliente en Node.js

```javascript
const { connect, StringCodec } = require('nats');

async function main() {
    const nc = await connect({ servers: 'nats://localhost:4222' });
    const sc = StringCodec();
    
    // Enviar comando
    const request = JSON.stringify({ command: 'status' });
    const response = await nc.request('freeswitch.api', sc.encode(request), { timeout: 5000 });
    
    // Procesar respuesta
    const data = JSON.parse(sc.decode(response.data));
    console.log('Success:', data.success);
    console.log('Data:', data.data);
    
    await nc.close();
}

main();
```

### Ejemplo 4: CLI con curl-like usando simple_test

```bash
# Status del sistema
LD_LIBRARY_PATH=./lib/nats ./tests/bin/simple_test req freeswitch.api '{"command":"status"}'

# Versión
LD_LIBRARY_PATH=./lib/nats ./tests/bin/simple_test req freeswitch.api '{"command":"version"}'

# Variable global
LD_LIBRARY_PATH=./lib/nats ./tests/bin/simple_test req freeswitch.api '{"command":"global_getvar","args":"hostname"}'
```

---

## 🚨 Manejo de Errores

### Error: JSON Inválido

**Request:**
```
This is not JSON
```

**Response:**
```json
{
  "success": false,
  "message": "Invalid JSON format",
  "data": null,
  "timestamp": 1764893800000000,
  "node_id": "fs-node-01"
}
```

### Error: Comando Faltante

**Request:**
```json
{
  "args": "something"
}
```

**Response:**
```json
{
  "success": false,
  "message": "Missing 'command' field",
  "data": null,
  "timestamp": 1764893810000000,
  "node_id": "fs-node-01"
}
```

### Error: Comando Inválido

**Request:**
```json
{
  "command": "nonexistent_command"
}
```

**Response:**
```json
{
  "success": false,
  "message": "API command failed",
  "data": "-ERR Command not found!",
  "timestamp": 1764893820000000,
  "node_id": "fs-node-01"
}
```

---

## 🔒 Consideraciones de Seguridad

1. **Autenticación NATS**: Configure autenticación en NATS Server
2. **TLS/SSL**: Use conexiones seguras en producción (`nats://` → `tls://`)
3. **ACLs**: Restrinja qué clientes pueden publicar en `freeswitch.*`
4. **Rate Limiting**: Implemente limitación de tasa en el broker
5. **Input Validation**: Valide todos los comandos antes de ejecutar

---

## 📈 Performance Tips

1. **Connection Pooling**: Reutilice conexiones NATS
2. **Batch Requests**: Agrupe múltiples comandos cuando sea posible
3. **Async Commands**: Use comandos asíncronos para operaciones fire-and-forget
4. **Timeout Adecuado**: Configure timeouts según su red (recomendado: 5-10s)
5. **Request Buffering**: Implemente buffering en cliente para alta carga

---

## 🔗 Referencias

- **FreeSWITCH API**: https://freeswitch.org/confluence/display/FREESWITCH/mod_commands
- **NATS Protocol**: https://docs.nats.io/reference/reference-protocols/nats-protocol
- **JSON Specification**: https://www.json.org/

---

**Última actualización**: Diciembre 2025
