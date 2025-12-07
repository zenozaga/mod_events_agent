# FreeSWITCH Call Control - Vanilla JS

Pure HTML/CSS/JavaScript web interface + Native Node.js API to control FreeSWITCH via mod_event_agent.

## 🚀 Installation

```bash
cd example
npm install
```

## ▶️ Run

```bash
node server.js
```

Open browser: **http://localhost:3000**

## 🌐 API Endpoints

### Real-time Events
- `GET /api/events` - Server-Sent Events stream

### FreeSWITCH Commands
- `POST /api/command` - Execute FreeSWITCH command
  ```json
  { "command": "show", "args": "calls" }
  ```

### Call Control
- `GET /api/calls` - Get active calls
- `POST /api/calls/answer` - Answer call
- `POST /api/calls/hangup` - Hangup call
- `POST /api/calls/transfer` - Transfer call
- `POST /api/calls/originate` - Originate new call

## ⚙️ Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | `3000` | HTTP server port |
| `NATS_URL` | `nats://localhost:4222` | NATS broker URL |
| `NODE_ID` | `fs_node_1` | FreeSWITCH node ID |

## 🎨 Features

- ✅ Real-time call dashboard
- ✅ Call control (Answer, Hangup, Transfer, Originate)
- ✅ Command console with history
- ✅ Connection status indicator
- ✅ Responsive design (Tailwind CSS via CDN)
- ✅ Single HTML file (no build process)

## 🧪 Testing

```bash
# View active calls
curl -X POST http://localhost:3000/api/command \
  -H "Content-Type: application/json" \
  -d '{"command":"show","args":"calls"}'

# Originate call
curl -X POST http://localhost:3000/api/calls/originate \
  -H "Content-Type: application/json" \
  -d '{"endpoint":"user/1001","destination":"5000"}'
```

## 📁 Project Structure

```
example/
├── server.js          # Native Node.js HTTP server
├── package.json       # Dependencies (nats only)
└── public/
    └── index.html     # Complete frontend (HTML + CSS + JS)
```

## 🔧 Technical Stack

- **Frontend**: Vanilla JavaScript, Tailwind CSS (CDN), EventSource API
- **Backend**: Node.js HTTP server (native, no Express)
- **Message Broker**: NATS client
- **No build process**: Just open the HTML file

curl -X POST http://localhost:3000/api/calls/hangup \
  -H "Content-Type: application/json" \
  -d '{"uuid":"abc-123-uuid"}'
```

### Ver eventos en vivo
```bash
curl -N http://localhost:3000/api/events
```

## 🔗 Integración con mod_event_agent

Este ejemplo se comunica con FreeSWITCH mediante:

1. **NATS Subjects** configurados:
   - `freeswitch.api.{NODE_ID}` - Comandos API
   - `freeswitch.events.*` - Eventos en tiempo real

2. **Configuración en FreeSWITCH**:
   ```xml
   <param name="driver" value="nats"/>
   <!-- Docker Internal network -->
   <param name="url" value="nats://localhost:4222"/>
   <param name="node-id" value="agent_node_1"/>
   ```
 
 