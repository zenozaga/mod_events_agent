# Examples - mod_event_agent

Practical client examples for interacting with FreeSWITCH using `mod_event_agent`.

---

## 📋 Available Examples

| Example | Language | Description | Difficulty |
|---------|----------|-------------|------------|
| **nats_command_client** | C | Basic request-reply client | ⭐ Easy |
| **nats_subscriber** | C | Event subscriber (future) | ⭐ Easy |
| **call_monitor** | C | Real-time call monitor | ⭐⭐ Medium |
| **Integrated tests** | C | Complete test clients | ⭐⭐ Medium |

---

## 🚀 Quick Start

### Option 1: Use Test Clients (Recommended)

The most complete clients are in `/tests`:

```bash
cd tests
make

# Client that sends commands
LD_LIBRARY_PATH=../lib/nats ./bin/service_a_nats '{"command":"status"}'

# Interactive client
LD_LIBRARY_PATH=../lib/nats ./bin/service_a_nats
> {"command":"version"}
> {"command":"show","args":"channels"}
> quit

# Multi-mode client
LD_LIBRARY_PATH=../lib/nats ./bin/simple_test req freeswitch.api '{"command":"status"}'
```

### Option 2: Compile Examples

```bash
cd examples
make

# Basic client
./nats_command_client
```

---

## 📖 Detailed Examples

### 1. nats_command_client.c

Basic C client demonstrating request-reply pattern.

**Features:**
- NATS connection
- JSON command sending
- Response reception
- Error handling

**Code:**
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

**Compile:**
```bash
gcc -o nats_command_client nats_command_client.c \
    -I../lib/nats/include \
    -L../lib/nats -lnats \
    -Wl,-rpath,../lib/nats
```

**Execute:**
```bash
./nats_command_client
```

---

### 2. nats_subscriber.c

FreeSWITCH event subscriber (prepared for future implementation).

**Features:**
- Event subscription
- JSON deserialization
- Real-time processing

**Future usage:**
```bash
./nats_subscriber freeswitch.events.CHANNEL_CREATE
./nats_subscriber "freeswitch.events.*"
```

---

### 3. call_monitor.c

Complete real-time call monitor.

**Features:**
- Active calls dashboard
- Real-time statistics
- Important event alerts

**Compile:**
```bash
cd examples
make call_monitor
```

**Execute:**
```bash
./call_monitor nats://localhost:4222
```

---

## 💡 Practical Use Cases

### Case 1: Simple CLI

Create a CLI for quick FreeSWITCH control:

```bash
#!/bin/bash
# fs-cli: Simple CLI for FreeSWITCH

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

**Use:**
```bash
chmod +x fs-cli
./fs-cli status
./fs-cli channels
```

---

### Case 2: Call Monitor

Simple dashboard in Python:

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
        # Get active calls
        request = json.dumps({"command": "show", "args": "calls"})
        response = await nc.request("freeswitch.api", request.encode(), timeout=2)
        
        data = json.loads(response.data.decode())
        if data['success']:
            print(f"\n[{datetime.now()}] Active Calls:")
            print(data['data'])
        
        await asyncio.sleep(5)  # Update every 5 seconds
    
    await nc.close()

if __name__ == '__main__':
    asyncio.run(monitor_calls())
```

---

### Case 3: Webhook Processor

Process webhooks and execute commands in FreeSWITCH:

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

### Case 4: CRM Integration

Synchronize calls with CRM:

```python
#!/usr/bin/env python3
import asyncio
from nats.aio.client import Client as NATS
import json
import requests

CRM_API = "https://crm.example.com/api/calls"

async def log_call_to_crm(call_data):
    """Send call information to CRM"""
    response = requests.post(CRM_API, json=call_data)
    return response.status_code == 200

async def originate_from_crm(customer_phone, agent_extension):
    """Originate call from CRM"""
    nc = NATS()
    await nc.connect("nats://localhost:4222")
    
    request = json.dumps({
        "command": "originate",
        "args": f"user/{agent_extension} {customer_phone}"
    })
    
    response = await nc.request("freeswitch.api", request.encode(), timeout=5)
    data = json.loads(response.data.decode())
    
    if data['success']:
        # Log in CRM
        await log_call_to_crm({
            'agent': agent_extension,
            'customer': customer_phone,
            'uuid': data['data'],
            'status': 'initiated'
        })
    
    await nc.close()
    return data

# Usage from CRM webhook:
# POST /api/originate {"customer_phone":"5551234567","agent_extension":"1000"}
```

---

## 🔧 Example Compilation

### Makefile for Examples

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

**Compile:**
```bash
cd examples
make
```

---

## 📚 References

- **[docs/API.md](../docs/API.md)**: Complete API documentation
- **[tests/](../tests/)**: More complete test clients
- **[README.md](../README.md)**: Main module documentation

---

## 🐛 Troubleshooting

### Error: "Cannot connect to NATS"

```bash
# Verify NATS is running
docker ps | grep nats

# Or check port
netstat -an | grep 4222
```

### Error: "Shared library not found"

```bash
# Add to LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/path/to/lib/nats:$LD_LIBRARY_PATH

# Or use RPATH in compilation
gcc ... -Wl,-rpath,/path/to/lib/nats
```

### Error: "No response from FreeSWITCH"

```bash
# Verify mod_event_agent is loaded
docker exec -it freeswitch fs_cli -x "module_exists mod_event_agent"

# View logs
docker logs freeswitch | grep event_agent
```

---

**Last updated**: December 2025

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
