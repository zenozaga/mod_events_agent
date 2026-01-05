# API Reference - mod_event_agent

Complete API documentation for remote FreeSWITCH control via `mod_event_agent`.

---

## 📋 Table of Contents

- [Communication Architecture](#communication-architecture)
- [Message Format](#message-format)
- [Subject Patterns](#subject-patterns)
- [API Commands](#api-commands)
  - [Core Commands](#core-commands)
  - [Call Control Commands](#call-control-commands)
  - [Dialplan Control Commands](#dialplan-control-commands)
- [Event Streaming](#event-streaming)
- [Response Codes](#response-codes)
- [Error Handling](#error-handling)
- [Usage Examples](#usage-examples)

---

## 🏗️ Communication Architecture

### Request-Reply Pattern (Commands)

All commands use synchronous request-reply for guaranteed delivery and response:

```
┌──────────┐                      ┌──────────┐                    ┌────────────┐
│  Client  │                      │   NATS   │                    │ FreeSWITCH │
└────┬─────┘                      └────┬─────┘                    └─────┬──────┘
     │                                 │                                │
    │ 1. Request(freeswitch.cmd.originate)      │                        │
     │    + Reply subject              │                                │
     ├────────────────────────────────>│                                │
     │                                 │ 2. Route to subscriber         │
     │                                 ├───────────────────────────────>│
     │                                 │                                │
     │                                 │ 3. Execute & build response    │
     │                                 │<───────────────────────────────┤
     │                                 │                                │
     │ 4. Response on reply subject    │                                │
     │<────────────────────────────────┤                                │
     │    {success: true, ...}         │                                │
     └─────────────────────────────────┴────────────────────────────────┘
```

### Pub/Sub Pattern (Events)

Events are published without expecting responses:

```
┌────────────┐                    ┌──────────┐                    ┌────────────┐
│ FreeSWITCH │                    │   NATS   │                    │ Subscriber │
└─────┬──────┘                    └────┬─────┘                    └─────┬──────┘
      │                                │                                │
      │ 1. Publish event               │                                │
      │   freeswitch.events.channel.answer     │                                │
      ├───────────────────────────────>│                                │
      │                                │ 2. Deliver to all subscribers  │
      │                                ├───────────────────────────────>│
      │                                │                                │
      │ 3. Continue processing         │ 4. Process event               │
      │                                │                                │
      └────────────────────────────────┴────────────────────────────────┘
```

---

## 📦 Message Format

### Request (Client → FreeSWITCH)

```json
{
  "command": "string",      // API command or action (required)
  "args": "string",         // Command arguments (optional)
  "node_id": "string",      // Target node for broadcast subjects (optional)
  
  // Command-specific fields (varies by command)
  "endpoint": "string",     // For call.originate
  "destination": "string",  // For call.originate
  "uuid": "string",         // For hangup / uuid_* API helpers
  "mode": "string",         // For dialplan.audio
  "enabled": boolean        // For dialplan.autoanswer
}
```

### Response (FreeSWITCH → Client)

```json
{
  "success": boolean,       // true if successful, false if error
  "message": "string",      // Human-readable result message
  "data": "string|object",  // Command output (null if none)
  "timestamp": number,      // Unix timestamp in microseconds
  "node_id": "string"       // Node that processed the request
}
```

**Success Response Example**:
```json
{
  "success": true,
  "message": "Command executed successfully",
  "data": "UP 0 years, 1 days, 5 hours...",
  "timestamp": 1733433600000000,
  "node_id": "fs_node_01"
}
```

**Error Response Example**:
```json
{
  "success": false,
  "message": "Invalid command syntax",
  "data": null,
  "timestamp": 1733433600000000,
  "node_id": "fs_node_01"
}
```

---

## 🎯 Subject Patterns

### Broadcast Subscriptions (All Nodes)

Messages delivered to **all nodes**, filtered by `node_id` in JSON:

| Subject | Type | Description |
|---------|------|-------------|
| `freeswitch.api` | Request-Reply | Generic API commands |
| `freeswitch.cmd.*` | Request-Reply | Structured commands |
| `freeswitch.events.*` | Pub/Sub | Event streaming |

**Example**:
```json
// Sent to: freeswitch.api
{
  "command": "status",
  "node_id": "fs_node_01"  // Only this node processes
}
```

### Direct Subscriptions (Specific Node)

Messages delivered **directly to one node** via NATS routing:

| Subject Pattern | Type | Description |
|----------------|------|-------------|
| `freeswitch.api.{node_id}` | Request-Reply | Direct API to specific node |
| `freeswitch.cmd.*.{node_id}` | Request-Reply | Direct commands to specific node |

**Example**:
```bash
# Subject: freeswitch.api.fs_node_01 (no node_id in JSON needed)
{"command": "status"}
```

**Node ID Slugification**:
- Uppercase → lowercase
- `-`, `.`, `/`, ` ` → `_`
- Non-alphanumeric → `_`

Examples:
- `FS-Node-01` → `fs_node_01`
- `freeswitch.node.02` → `fs_node_02`

---

## 🎯 API Commands

### Core Commands

#### 1. Generic API Execution

Execute any FreeSWITCH API command.

**Subject**: `freeswitch.api[.{node_id}]`

**Request**:
```json
{
  "command": "status",           // Any FS API command
  "args": "",                    // Optional arguments
  "node_id": "fs_node_01"        // Optional (broadcast only)
}
```

**Response**:
```json
{
  "success": true,
  "message": "Command executed successfully",
  "data": "UP 0 years, 1 days, 5 hours, 32 minutes...",
  "timestamp": 1733433600000000,
  "node_id": "fs_node_01"
}
```

**Examples**:
```json
// Get channel list
{"command": "show", "args": "channels"}

// Reload XML
{"command": "reloadxml"}

// Get call count
{"command": "show", "args": "calls count"}
```

#### 2. Module Statistics

Get mod_event_agent statistics and health. You can also change the module log level on the fly by including a `log_level` field in the request payload.

**Subject**: `freeswitch.cmd.status[.{node_id}]`

**Request**: `{}` (status only) or:

```json
{"log_level": "debug"}
```

**Response**:
```json
{
  "success": true,
  "status": "success",
  "message": "System status",
  "timestamp": 1733433600000000,
  "node_id": "fs-node-01",
  "data": {
    "version": "2.0.0",
    "log_level": "info",
    "stats": {
      "requests_received": 5432,
      "requests_success": 5400,
      "requests_failed": 32
    }
  }
}
```

If the level changes successfully the response also contains `"log_level_updated": true` inside `data` so clients can confirm the new verbosity is active.

---

### Call Control Commands

#### 3. Originate Call

Create an outbound call with full control.

**Subject**: `freeswitch.cmd.originate`

**Request**:
```json
{
  "endpoint": "user/1000",                    // Required: endpoint to dial
  "destination": "&park",                     // Required: destination application
  "caller_id_name": "Bot Call",              // Optional
  "caller_id_number": "5551234",             // Optional
  "timeout": 60,                              // Optional: ring timeout (seconds)
  "variables": {                              // Optional: channel variables
    "custom_var": "value",
    "sip_h_X-Custom": "header_value"
  }
}
```

**Response**:
```json
{
  "success": true,
  "message": "Call originated successfully",
  "data": {
    "uuid": "abc-123-def-456",
    "endpoint": "user/1000",
    "destination": "&park"
  },
  "timestamp": 1733433600000000,
  "node_id": "fs_node_01"
}
```

**Common Endpoints**:
- `user/1000` - Local extension
- `sofia/gateway/provider/5551234` - SIP trunk
- `sofia/internal/user@domain.com` - Direct SIP URI

**Common Destinations**:
- `&park` - Park call
- `&echo` - Echo test
- `9196` - Extension number
- `&bridge(sofia/gateway/provider/5551234)` - Immediate bridge

#### 4. Hangup Call

Terminate an active channel with an optional cause.

**Subject**: `freeswitch.cmd.hangup`

**Request**:
```json
{
  "uuid": "abc-123-def-456",      // Required: call UUID
  "cause": "NORMAL_CLEARING"      // Optional: hangup cause
}
```

**Response**:
```json
{
  "success": true,
  "message": "Channel hangup successful",
  "data": "abc-123-def-456",
  "timestamp": 1733433600000000,
  "node_id": "fs_node_01"
}
```

> Need to bridge, transfer o contestar una llamada? Usa `freeswitch.api` con comandos nativos como `uuid_bridge`, `uuid_transfer`, `uuid_answer` o `uuid_broadcast`.

#### 5. Async Variants

Para cargas altas puedes usar los temas asincrónicos que no envían respuesta:

| Subject | Acción |
|---------|--------|
| `freeswitch.cmd.async.originate` | Origina sin esperar respuesta |
| `freeswitch.cmd.async.hangup` | Cuelga sin esperar respuesta |

Publica el mismo payload que las versiones síncronas; cualquier error se registra en FreeSWITCH.

### Dialplan Control Commands

#### 6. Enable Park Mode

Intercept all inbound calls and park them.

**Subject**: `freeswitch.cmd.dialplan.enable`

**Request**: `{}`

**Response**:
```json
{
  "success": true,
  "message": "Park mode enabled",
  "mode": "park",
  "timestamp": 1733433600000000,
  "node_id": "fs_node_01"
}
```

#### 7. Disable Park Mode

Return to normal dialplan processing.

**Subject**: `freeswitch.cmd.dialplan.disable`

**Request**: `{}`

**Response**:
```json
{
  "success": true,
  "message": "Park mode disabled",
  "mode": "disabled",
  "timestamp": 1733433600000000,
  "node_id": "fs_node_01"
}
```

#### 8. Set Audio Mode

Configure audio during park.

**Subject**: `freeswitch.cmd.dialplan.audio`

**Request**:
```json
{
  "mode": "ringback",                    // Required: silence|ringback|music
  "music_class": "moh"                   // Optional: MOH class (music mode only)
}
```

**Audio Modes**:
- `silence` - No audio
- `ringback` - Ring tone (US)
- `music` - Music on hold

**Response**:
```json
{
  "success": true,
  "message": "Audio mode updated",
  "mode": "ringback",
  "timestamp": 1733433600000000,
  "node_id": "fs_node_01"
}
```

#### 9. Configure Auto-Answer

Enable/disable automatic answer on park.

**Subject**: `freeswitch.cmd.dialplan.autoanswer`

**Request**:
```json
{
  "enabled": true                        // Required: boolean
}
```

**Response**:
```json
{
  "success": true,
  "message": "Auto-answer updated",
  "enabled": true,
  "timestamp": 1733433600000000,
  "node_id": "fs_node_01"
}
```

#### 10. Get Dialplan Status

Get current dialplan manager configuration.

**Subject**: `freeswitch.cmd.dialplan.status`

**Request**: `{}`

**Response**:
```json
{
  "success": true,
  "info": "Mode: park | Audio: ringback | Auto-answer: enabled | Calls parked: 5",
  "timestamp": 1733433600000000,
  "node_id": "fs_node_01"
}
```

---

## 📡 Event Streaming

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
#### 4. Hangup Call

Terminate a specific UUID with an optional cause.

**Subject**: `freeswitch.cmd.hangup`

**Request**:
```json
{
  "uuid": "abc-123-def-456",                // Required: call UUID
  "cause": "NORMAL_CLEARING"               // Optional: hangup cause
}
```

**Response**:
```json
{
  "success": true,
  "message": "Channel hangup successful",
  "data": "abc-123-def-456",
  "timestamp": 1733433600000000,
  "node_id": "fs_node_01"
}
```

#### 5. Async Commands

Fire-and-forget variants for high-throughput scenarios.

| Subject | Description | Reply |
|---------|-------------|-------|
| `freeswitch.cmd.async.originate` | Originate without waiting for response | ✖ |
| `freeswitch.cmd.async.hangup` | Hangup without waiting | ✖ |

These commands share the same payloads as their synchronous counterparts but do not return responses. Monitor FreeSWITCH events or application logs for completion.

> ℹ️ Need to bridge or transfer calls? Use `freeswitch.api` with native commands such as `uuid_bridge`, `uuid_transfer`, `uuid_broadcast`, etc.
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

#### Broadcast Requests (with JSON node_id filtering)
```bash
# System status - broadcast to all nodes, only node_id="fs_node_01" processes it
LD_LIBRARY_PATH=./lib/nats ./tests/bin/simple_test req freeswitch.api '{"command":"status","node_id":"fs_node_01"}'

# Version - broadcast without node_id, first available node processes it
LD_LIBRARY_PATH=./lib/nats ./tests/bin/simple_test req freeswitch.api '{"command":"version"}'

# Global variable - targeted to specific node
LD_LIBRARY_PATH=./lib/nats ./tests/bin/simple_test req freeswitch.api '{"command":"global_getvar","args":"hostname","node_id":"fs_node_02"}'
```

#### Direct Requests (NATS routes to specific node, no JSON filtering)
```bash
# System status - direct to fs_node_01 (more efficient, no network overhead for other nodes)
LD_LIBRARY_PATH=./lib/nats ./tests/bin/simple_test req freeswitch.api.fs_node_01 '{"command":"status"}'

# Version - direct to fs_node_02
LD_LIBRARY_PATH=./lib/nats ./tests/bin/simple_test req freeswitch.api.fs_node_02 '{"command":"version"}'

# Global variable - direct to specific node (no node_id needed in JSON)
LD_LIBRARY_PATH=./lib/nats ./tests/bin/simple_test req freeswitch.api.fs_node_01 '{"command":"global_getvar","args":"hostname"}'
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

## 🎛️ Dialplan Control Commands

**NEW**: Dynamic dialplan control for intercepting and managing inbound calls without editing XML files.

### Enable Park Mode

**Subject:** `freeswitch.cmd.dialplan.enable`

Enables park mode. All inbound calls will be intercepted and parked until you decide what to do with them.

**Request:**
```json
{}
```

**Response:**
```json
{
  "status": "success",
  "message": "Park mode enabled",
  "mode": "park"
}
```

### Disable Park Mode

**Subject:** `freeswitch.cmd.dialplan.disable`

Disables park mode. Normal dialplan processing resumes.

**Request:**
```json
{}
```

**Response:**
```json
{
  "status": "success",
  "message": "Park mode disabled",
  "mode": "disabled"
}
```

### Set Audio Mode

**Subject:** `freeswitch.cmd.dialplan.audio`

Configures what audio the caller hears while parked.

**Request:**
```json
{
  "mode": "silence|ringback|music",
  "music_class": "moh"  // optional, only for music mode
}
```

**Audio Modes:**
- `silence`: No audio, caller hears silence
- `ringback`: Caller hears ringback tone (ring-ring)
- `music`: Caller hears music on hold

**Response:**
```json
{
  "status": "success",
  "message": "Audio mode updated",
  "mode": "ringback"
}
```

### Set Auto-Answer

**Subject:** `freeswitch.cmd.dialplan.autoanswer`

Enables or disables automatic answering of parked calls.

**Request:**
```json
{
  "enabled": true
}
```

**Response:**
```json
{
  "status": "success",
  "message": "Auto-answer updated",
  "enabled": true
}
```

### Get Dialplan Status

**Subject:** `freeswitch.cmd.dialplan.status`

Returns current dialplan configuration and statistics.

**Request:**
```json
{}
```

**Response:**
```json
{
  "status": "success",
  "info": "Dialplan Manager Status:\n  Mode: PARK\n  Audio Mode: RINGBACK\n  Auto Answer: NO\n  Context: default\n  Music Class: moh\n  Calls Intercepted: 42\n  Calls Parked: 42\n"
}
```

### Use Cases

**Scenario 1: Queue with Custom Logic**
```python
# Enable park with music
await nats.publish("freeswitch.cmd.dialplan.enable", "{}")
await nats.publish("freeswitch.cmd.dialplan.audio", '{"mode":"music"}')
await nats.publish("freeswitch.cmd.dialplan.autoanswer", '{"enabled":true}')

# Your app receives CHANNEL_PARK events
# Analyze and route: uuid_transfer, uuid_bridge, etc.
```

**Scenario 2: Business Hours**
```python
if is_business_hours():
    await nats.publish("freeswitch.cmd.dialplan.disable", "{}")
else:
    await nats.publish("freeswitch.cmd.dialplan.enable", "{}")
    await nats.publish("freeswitch.cmd.dialplan.audio", '{"mode":"music"}')
```

**Complete documentation:** See [DIALPLAN_CONTROL.md](DIALPLAN_CONTROL.md)

---

## 🔒 Security Considerations

1. **NATS Authentication**: Configure authentication on NATS Server
2. **TLS/SSL**: Use secure connections in production (`nats://` → `tls://`)
3. **ACLs**: Restrict which clients can publish to `freeswitch.*`
4. **Rate Limiting**: Implement rate limiting on the broker
5. **Input Validation**: Validate all commands before execution

---

## 📈 Performance Tips

1. **Use Direct Subscriptions**: When targeting a specific node, use direct subscriptions (`freeswitch.api.{node_id}`) instead of broadcast with JSON filtering. This reduces network overhead as NATS routes messages only to the target node.
2. **Broadcast for Failover**: Use broadcast subscriptions (`freeswitch.api`) without `node_id` when you want any available node to process the request (automatic load balancing).
3. **Connection Pooling**: Reuse NATS connections
4. **Batch Requests**: Group multiple commands when possible
5. **Async Commands**: Use asynchronous commands for fire-and-forget operations
6. **Adequate Timeout**: Configure timeouts according to your network (recommended: 5-10s)
7. **Request Buffering**: Implement buffering in client for high load

### Performance Comparison

| Scenario | Subject | Network Cost | Use Case |
|----------|---------|--------------|----------|
| Any available node | `freeswitch.api` | O(n) - all nodes receive | Load balancing, failover |
| Specific node (broadcast) | `freeswitch.api` + `"node_id":"fs_node_01"` | O(n) - all nodes receive, filter in app | Legacy compatibility |
| Specific node (direct) | `freeswitch.api.fs_node_01` | O(1) - only target receives | **Recommended** for targeted requests |

---

## 🔗 References

- **FreeSWITCH API**: https://freeswitch.org/confluence/display/FREESWITCH/mod_commands
- **NATS Protocol**: https://docs.nats.io/reference/reference-protocols/nats-protocol
- **JSON Specification**: https://www.json.org/
- **Dialplan Control**: [DIALPLAN_CONTROL.md](DIALPLAN_CONTROL.md)

---

**Last updated**: December 2025
