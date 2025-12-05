# API Reference - mod_event_agent

Complete API documentation for FreeSWITCH control via `mod_event_agent`.

---

## 📋 Table of Contents

- [Communication Architecture](#communication-architecture)
- [Message Format](#message-format)
- [API Commands](#api-commands)
- [Response Codes](#response-codes)
- [Usage Examples](#usage-examples)
- [Error Handling](#error-handling)

---

## 🏗️ Communication Architecture

### Request-Reply Pattern

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

### Available Subjects (Topics)

| Subject | Type | Description |
|---------|------|-------------|
| `freeswitch.api` | Request-Reply | Synchronous commands with response |
| `freeswitch.cmd.async.*` | Fire-and-Forget | Asynchronous commands without response |
| `freeswitch.events.*` | Pub/Sub | FreeSWITCH events (future) |

---

## 📦 Message Format

### Request (Client → FreeSWITCH)

```json
{
  "command": "string",      // FreeSWITCH API command (required)
  "args": "string",         // Command arguments (optional)
  "node_id": "string"       // Target node ID for clusters (optional)
}
```

#### Fields

- **`command`** (string, required): FreeSWITCH API command name
- **`args`** (string, optional): Additional command arguments
- **`node_id`** (string, optional): Specific node ID in multi-node clusters

### Response (FreeSWITCH → Client)

```json
{
  "success": boolean,       // true if command executed successfully
  "message": "string",      // Descriptive result message
  "data": "string",         // Command response (can be null)
  "timestamp": number,      // Unix timestamp in microseconds
  "node_id": "string"       // ID of the node that processed the command
}
```

#### Fields

- **`success`** (boolean): Indicates if command executed without errors
- **`message`** (string): Description of result or error
- **`data`** (string): FreeSWITCH command output (format depends on command)
- **`timestamp`** (number): Unix timestamp in microseconds
- **`node_id`** (string): FreeSWITCH node identifier that processed the command

---

## 🎯 API Commands

### System Commands

#### `status`
Get FreeSWITCH system status.

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
Get FreeSWITCH version.

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
Get system uptime.

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

### Variable Commands

#### `global_getvar`
Get global variable value.

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
Set a global variable.

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

### Information Commands

#### `show modules`
List all loaded modules.

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
List all active channels.

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
List all active calls.

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

### SIP Commands (Sofia)

#### `sofia status`
Get status of all SIP profiles.

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
Get status of a specific SIP profile.

**Request:**
```json
{
  "command": "sofia",
  "args": "status profile drachtio_mrf"
}
```

---

### Call Commands

#### `originate`
Originate a new call.

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
Terminate a call by UUID.

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
Terminate all active calls.

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

### Module Commands

#### `load`
Load a module dynamically.

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
Unload a module.

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
Reload a module.

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

## 📊 Response Codes

### Success States

| State | `success` | Description |
|--------|-----------|-------------|
| Command executed | `true` | Command executed successfully |
| Command without output | `true` | Successful command but no return data (`data: null`) |

### Error States

| State | `success` | `message` | Description |
|--------|-----------|-----------|-------------|
| Invalid JSON | `false` | `"Invalid JSON format"` | Payload is not valid JSON |
| Missing field | `false` | `"Missing 'command' field"` | `command` field missing in JSON |
| Invalid command | `false` | `"API command failed"` | Command doesn't exist or failed execution |
| Internal error | `false` | `"Internal error"` | Module or FreeSWITCH internal error |

---

## 💡 Usage Examples

### Example 1: Basic C Client

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

### Example 2: Python Client

```python
import asyncio
from nats.aio.client import Client as NATS
import json

async def main():
    nc = NATS()
    await nc.connect("nats://localhost:4222")
    
    # Send command
    request = json.dumps({"command": "status"})
    response = await nc.request("freeswitch.api", request.encode(), timeout=5)
    
    # Process response
    data = json.loads(response.data.decode())
    print(f"Success: {data['success']}")
    print(f"Data: {data['data']}")
    
    await nc.close()

if __name__ == '__main__':
    asyncio.run(main())
```

### Example 3: Node.js Client

```javascript
const { connect, StringCodec } = require('nats');

async function main() {
    const nc = await connect({ servers: 'nats://localhost:4222' });
    const sc = StringCodec();
    
    // Send command
    const request = JSON.stringify({ command: 'status' });
    const response = await nc.request('freeswitch.api', sc.encode(request), { timeout: 5000 });
    
    // Process response
    const data = JSON.parse(sc.decode(response.data));
    console.log('Success:', data.success);
    console.log('Data:', data.data);
    
    await nc.close();
}

main();
```

### Example 4: CLI with curl-like using simple_test

```bash
# System status
LD_LIBRARY_PATH=./lib/nats ./tests/bin/simple_test req freeswitch.api '{"command":"status"}'

# Version
LD_LIBRARY_PATH=./lib/nats ./tests/bin/simple_test req freeswitch.api '{"command":"version"}'

# Global variable
LD_LIBRARY_PATH=./lib/nats ./tests/bin/simple_test req freeswitch.api '{"command":"global_getvar","args":"hostname"}'
```

---

## 🚨 Error Handling

### Error: Invalid JSON

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

### Error: Missing Command

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

### Error: Invalid Command

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

## 🔒 Security Considerations

1. **NATS Authentication**: Configure authentication on NATS Server
2. **TLS/SSL**: Use secure connections in production (`nats://` → `tls://`)
3. **ACLs**: Restrict which clients can publish to `freeswitch.*`
4. **Rate Limiting**: Implement rate limiting on the broker
5. **Input Validation**: Validate all commands before execution

---

## 📈 Performance Tips

1. **Connection Pooling**: Reuse NATS connections
2. **Batch Requests**: Group multiple commands when possible
3. **Async Commands**: Use asynchronous commands for fire-and-forget operations
4. **Adequate Timeout**: Configure timeouts according to your network (recommended: 5-10s)
5. **Request Buffering**: Implement buffering in client for high load

---

## 🔗 References

- **FreeSWITCH API**: https://freeswitch.org/confluence/display/FREESWITCH/mod_commands
- **NATS Protocol**: https://docs.nats.io/reference/reference-protocols/nats-protocol
- **JSON Specification**: https://www.json.org/

---

**Last updated**: December 2025
