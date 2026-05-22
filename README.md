# mod_event_agent - FreeSWITCH Event & Command Hub

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
[![License](https://img.shields.io/badge/license-MIT-blue)]()
[![FreeSWITCH](https://img.shields.io/badge/FreeSWITCH-1.10+-blue)]()
[![NATS](https://img.shields.io/badge/NATS-Ready-green)]()

**Production-ready FreeSWITCH module that transforms your PBX into a cloud-native microservice with real-time event streaming and remote command execution via NATS message broker.**

---

## 📖 Overview

`mod_event_agent` is a high-performance FreeSWITCH module that enables:

- 🎯 **Remote API Control**: Execute any FreeSWITCH command from external services
- 📡 **Real-Time Event Streaming**: Publish FreeSWITCH events to message brokers
- 🎛️ **Dynamic Dialplan Control**: Park/unpark calls with audio modes (silence, ringback, music)
- 🔄 **Bidirectional Communication**: Request-reply and pub/sub patterns
- 🌐 **Multi-Node Support**: Cluster-aware with node identification
- 🚀 **Production Performance**: 10k+ commands/sec, <1ms latency

### Key Use Cases

- **Call Center Integration**: Control FreeSWITCH from CRM/ERP systems
- **Smart IVR**: Dynamic dialplan management from external business logic
- **Real-Time Analytics**: Stream call events to data pipelines
- **Multi-Tenant Systems**: Isolated control per tenant with node routing
- **WebRTC Gateways**: Bridge SIP/WebRTC with external signaling

### Standard Response Envelope

Every synchronous command reply comes in the same JSON envelope so client code can be minimal and safe:

```json
{
  "success": true,
  "status": "success",
  "message": "API command executed",
  "timestamp": 1736123456789012,
  "node_id": "fs-node-01",
  "data": "optional payload"
}
```

- `timestamp` is expressed in **microseconds** since epoch for maximum resolution.
- `node_id` is always present (or "unknown" if the node was not configured).
- Handlers can extend the payload with extra keys like `mode`, `enabled`, or `info`, but the envelope is guaranteed.

---

## 🏗️ Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                        NATS MESSAGE BROKER                        │
│                      (Pub/Sub + Request/Reply)                    │
│                                                                    │
│  Subjects:                                                         │
│  • freeswitch.api                        ← Broadcast command lane        │
│  • freeswitch.node.{node_id}             ← Direct node lane              │
│  • freeswitch.events.*                  → Events (pub/sub)               │
└────────┬──────────────────────────────────────────┬──────────────┘
         │                                          │
    ┌────▼─────────┐                       ┌────────▼──────────┐
    │   Clients    │                       │  Event Consumers  │
    │              │                       │                   │
    │ • Python     │                       │ • Analytics       │
    │ • Node.js    │                       │ • CDR Processing  │
    │ • Go/Java    │                       │ • Monitoring      │
    │ • Any Lang   │                       │ • ML Pipelines    │
    └──────────────┘                       └───────────────────┘
         ▲                                          ▲
         │                                          │
┌────────┴──────────────────────────────────────────┴──────────────┐
│                      mod_event_agent                              │
│  ┌──────────────┐  ┌───────────────┐  ┌──────────────────────┐  │
│  │   Commands   │  │    Events     │  │      Dialplan        │  │
│  │   Handler    │  │   Adapter     │  │      Manager         │  │
│  │              │  │               │  │                      │  │
│  │ • API calls  │  │ • Streaming   │  │ • Park mode          │  │
│  │ • Originate  │  │ • Filtering   │  │ • Audio control      │  │
│  │ • Bridge     │  │ • JSON format │  │ • Dynamic XML        │  │
│  └──────┬───────┘  └───────┬───────┘  └─────────┬────────────┘  │
│         │                  │                     │               │
│         └──────────────────┴─────────────────────┘               │
│                            │                                     │
│                   ┌────────▼────────┐                            │
│                   │  NATS Driver    │                            │
│                   │  • Pub/Sub      │                            │
│                   │  • Req/Reply    │                            │
│                   │  • Auto-reconnect│                           │
│                   └─────────────────┘                            │
└───────────────────────────┬──────────────────────────────────────┘
                            │
                 ┌──────────▼───────────┐
                 │   FreeSWITCH Core    │
                 │   • Event System     │
                 │   • API Engine       │
                 │   • XML Dialplan     │
                 │   • Call Processing  │
                 └──────────────────────┘
```

---

## 🔁 Command Routing

- Publish to **`freeswitch.api`** for broadcast commands. Optionally add `"node_id":"fs-node-01"` in the payload to have a single node pick it up.
- Publish to **`freeswitch.node.{node_id}`** when you want to address a specific FreeSWITCH node directly (no `node_id` field required).
- Every payload must include a `command` string. Built-in handlers cover `originate`, `hangup`, `dialplan.enable`, `dialplan.disable`, `dialplan.audio`, `dialplan.autoanswer`, `dialplan.status`, and `agent.status`. Any other value falls back to native FreeSWITCH `api` execution, so `{"command":"show","args":"channels"}` still works.
- Add `"async": true` to make any command fire-and-forget. The request will be executed but no reply will be published; errors are still logged server-side for observability.

This registry-driven approach keeps clients simple (only two subjects to remember) while letting the server retain full validation, RBAC, and telemetry per command name.

---

## ✨ Features

### 🎯 Remote Control Commands

#### 1. Generic API Execution
Execute **any** FreeSWITCH API command remotely:
```bash
nats req freeswitch.api '{"command":"status"}'
nats req freeswitch.api '{"command":"show","args":"channels"}'
nats req freeswitch.api '{"command":"reloadxml"}'
```

#### 2. Call Origination
Create outbound calls with full control:
```json
{
  "command": "originate",
  "endpoint": "user/1000",
  "destination": "&park",
  "caller_id_name": "Bot",
  "caller_id_number": "5551234",
  "variables": {"custom_var": "value"}
}
```

#### 3. Call Bridging
Connect two legs dynamically using native FreeSWITCH commands:
```json
{
  "command": "uuid_bridge",
  "args": "abc-123-uuid sofia/gateway/provider/5551234"
}
```

#### 4. Dialplan App Execution (`execute_app`)
Invoke any FreeSWITCH **dialplan APP** (e.g. `play_and_get_digits`, `bridge`,
`ivr`, `record`, `playback`, `set`, `transfer`) on a live channel — without
the dialplan XML. APPs are registered via `SWITCH_ADD_APP` (distinct from
API verbs); the generic api handler cannot reach them because
`switch_api_execute` only resolves API verbs. `execute_app` fills that gap
by calling `switch_core_session_execute_application_async` directly — the
same C entry point dialplan XML uses internally.

```json
{
  "command": "execute_app",
  "uuid":    "<channel-uuid>",
  "app":     "play_and_get_digits",
  "args":    "1 4 3 5000 # tone_stream://%(300,0,440) /invalid.wav my_digits \\d+ 2000 -"
}
```

Required: `uuid`, `app`. Optional: `args` (passed verbatim as a single
string — spaces in args are preserved exactly, no internal tokenization).

**Dispatch is always async.** The reply confirms the app was queued on the
channel's runtime thread:

```json
{ "success": true, "message": "execute_app dispatched" }
```

The app's actual outcome (digits captured, file played, regex match, etc.)
flows back via the **event bus** as `CHANNEL_EXECUTE_COMPLETE`. Subscribe
to `freeswitch.events.channel.execute_complete` filtered by `Unique-ID`
and `Application` to observe completion. Why async: a sync wrapper would
block the module's single dispatch thread for the entire duration of the
app (potentially many seconds for `play_and_get_digits`), serializing all
other concurrent NATS commands behind it. Async returns in µs and keeps
the bus hot.

Failure modes:
- **Pre-dispatch** (missing uuid, missing app, channel not found, queue
  rejection) → reply `success:false` with a descriptive message.
- **Post-dispatch** (app rejected its args, regex didn't match, channel
  hung up mid-execution, internal app timeout) → reply was already
  `success:true`. Failure surfaces in the `CHANNEL_EXECUTE_COMPLETE`
  event headers (`Application-Response`, `variable_read_result`, etc.).

#### 5. Statistics & Monitoring
Real-time module metrics:
```json
{
  "command": "agent.status",
  "version": "2.0.0",
  "uptime": 3600,
  "events_published": 12345,
  "commands_received": 5432,
  "driver": "nats",
  "connected": true
}
```

### 🔀 `forward_to` — Tee side-channel for command responses

Every command accepts an optional `forward_to` field. When present, the
**same response envelope** that goes to the NATS request/reply inbox is
ALSO published to the supplied subject. Independent of `async`: a
fire-and-forget caller can still direct the result to an observer
(metrics, audit, side-branch workflows) without holding a request/reply
socket open.

```json
{
  "command": "execute_app",
  "uuid":    "abc-123",
  "app":     "play_and_get_digits",
  "args":    "1 4 3 5000 # ...",
  "forward_to": "metrics.voip.pagd.results"
}
```

Semantics:
- **Best-effort**: publish failures to `forward_to` are logged but never
  bubble up to the caller's reply. Caller's primary contract is unchanged.
- **Identical payload**: the bytes published to `forward_to` are byte-for-
  byte the same as the NATS inbox reply — predictable for observers.
- **Independent of `async`**: works when caller used `nc.Request()` (gets
  reply + forward) AND when caller used `nc.Publish()` with `async:true`
  (no reply, but forward still fires).
- **No correlation framing**: if the consumer needs to tie back to a
  specific request, include a `correlation_id` in the original payload and
  the handler will echo it in the response.

Common patterns:
- **Metrics fan-out**: `forward_to: "metrics.cmd.results"` — separate
  consumer collects latencies + outcomes without touching the caller path.
- **Audit trail**: `forward_to: "audit.fs.commands"` — durable log of
  every dispatched command across the cluster.
- **Workflow callback**: `forward_to: "workflow.callbacks.exec-42.node-3"`
  — engine resumes the suspended step when the response arrives.

### 🛡️ Payload Validation Helpers

Every built-in command now uses the lightweight validators under `src/validation/`. They provide
type-safe binding and descriptive errors without relying on giant schema files or runtime
allocations. The helpers follow the `v_<type>()` pattern and automatically write the sanitized
value into your payload struct:

```c
typedef struct {
  char endpoint[256];
  char extension[256];
} call_originate_payload_t;

call_originate_payload_t payload = {0};
const char *err = NULL;

if ((err = v_string(request->payload, &payload, endpoint,
          v_len(1, 255),
          "endpoint must be between 1 and 255 characters"))) {
  return command_result_error(err);
}

if ((err = v_enum(request->payload, &payload, mode,
          "mode must be silence, ringback, or music",
          "silence", "ringback", "music"))) {
  return command_result_error(err);
}
```

Available helpers:

- `v_string` / `v_string_opt` with `v_len`, `v_len_min`, `v_len_max`
- `v_number` / `v_number_opt` with `v_range` rules
- `v_bool` / `v_bool_opt`
- `v_enum` / `v_enum_opt`

They short-circuit on the first failure so command handlers stay tiny while clients receive human
readable messages.

### 🎛️ Dynamic Dialplan Control

Control call flow without reloading dialplan:

#### Park Mode with Audio Options
```bash
# Enable park with ringback tone
nats req freeswitch.api '{"command":"dialplan.enable"}'
nats req freeswitch.api '{"command":"dialplan.audio","mode":"ringback"}'

# Music on hold
nats req freeswitch.api '{"command":"dialplan.audio","mode":"music","music_class":"moh"}'

# Silent park
nats req freeswitch.api '{"command":"dialplan.audio","mode":"silence"}'

# Disable park (return to normal dialplan)
nats req freeswitch.api '{"command":"dialplan.disable"}'
```

#### Auto-Answer Configuration
```json
{
  "command": "dialplan.autoanswer",
  "enabled": true  // Auto-answer parked calls
}
```

**Use Cases**:
- Queue management (park until agent available)
- Call recording preparation
- IVR delays with custom audio
- Emergency broadcast mode

### 📊 Event Streaming

Stream FreeSWITCH events in real-time:

**Configurable Filtering**:
```xml
<param name="include-events" value="CHANNEL_CREATE,CHANNEL_DESTROY,CHANNEL_ANSWER"/>
<param name="exclude-events" value="HEARTBEAT,PRESENCE_IN"/>
```

**Event Format** (JSON):
```json
{
  "event_name": "CHANNEL_ANSWER",
  "timestamp": 1733433600000000,
  "node_id": "fs_node_01",
  "uuid": "abc-123-uuid",
  "headers": {
    "Caller-Destination-Number": "5551234",
    "Channel-State": "CS_EXECUTE"
  }
}
```

**Published to**: `freeswitch.events.channel.answer`, `freeswitch.events.channel.create`, etc.

#### ⚠️ Subject naming — underscore-to-dot conversion

The subject builder lowercases the FS event name AND converts every
`_` character to `.`. Subscribers MUST mirror that grammar or they
will silently miss events. Examples:

| FreeSWITCH event name (C constant) | NATS subject published |
|---|---|
| `CHANNEL_CREATE` | `<prefix>.events.channel.create` |
| `CHANNEL_ANSWER` | `<prefix>.events.channel.answer` |
| `CHANNEL_EXECUTE` | `<prefix>.events.channel.execute` |
| `CHANNEL_EXECUTE_COMPLETE` | `<prefix>.events.channel.execute.complete` |
| `CHANNEL_HANGUP` | `<prefix>.events.channel.hangup` |
| `CHANNEL_HANGUP_COMPLETE` | `<prefix>.events.channel.hangup.complete` |
| `DETECTED_SPEECH` | `<prefix>.events.detected.speech` |
| `RECORD_START` / `RECORD_STOP` | `<prefix>.events.record.start` / `.stop` |
| `RECV_INFO` | `<prefix>.events.recv.info` |

Common pitfall: subscribers writing `channel.execute_complete` (with
the underscore preserved) get NOTHING because the publisher path
emitted `channel.execute.complete` instead. Use NATS wildcards to
cover both intents when you don't want to track every dot manually:

```bash
# Everything channel-scoped (recommended for app-completion observers)
nats sub "freeswitch.events.channel.>"

# Only execute lifecycle (start + complete)
nats sub "freeswitch.events.channel.execute.>"
```

The conversion is implemented in `src/events/adapter.c::build_subject`:
```c
*p = tolower(*p);
if (*p == '_') {
    *p = '.';
}
```

This is intentional — dotted subjects play better with NATS subject
hierarchies and ACLs than underscores do — but it is NOT obvious from
the FS event name alone, hence this section.

### 🔗 Multi-Node Support

Route commands to specific nodes:

**Broadcast** (all nodes, filtered):
```json
{"command": "status", "node_id": "fs_node_01"}
```

**Direct** (specific node):
```bash
Subject: freeswitch.api.fs_node_01
Payload: {"command": "status"}
```

### 🚀 Performance Characteristics

| Metric | Value |
|--------|-------|
| **Command Throughput** | 10,000+ req/sec |
| **Latency (local)** | <1ms p99 |
| **Event Overhead** | <0.1% CPU |
| **Memory** | ~5MB baseline |
| **Network** | <100 KB/s idle |

---

## 🛡️ Security model

The module is designed for an **internal, trusted network** where the
NATS broker is the perimeter. Hardening lives at three layers, each
adding defence in depth:

### 1. Connection-level auth (env-driven)

Every credential is sourced from the environment, never from the XML
config (so a checked-in repo never leaks secrets):

| Env var | Purpose |
|---------|---------|
| `MOD_EVENT_AGENT_TOKEN` | NATS auth token (recommended) |
| `MOD_EVENT_AGENT_NKEY_SEED` | NATS NKey seed (alternative to token) |
| `MOD_EVENT_AGENT_URL` | Broker URL (e.g. `nats://nats:4222`) |
| `MOD_EVENT_AGENT_NODE_ID` | Per-node identifier |

Env values override anything declared in the XML. The token / nkey
fields in `event_agent.conf.xml` are deprecated and emit a warning
when populated.

### 2. Message-level guards (always on)

| Guard | What it does |
|-------|--------------|
| **Payload size cap** | Rejects any inbound JSON over 64 KB before parsing — backstop against an OOM-by-publish attack. |
| **Empty payload guard** | A NULL or zero-byte body is rejected with a distinct message instead of being mis-classified as bad JSON. |
| **Optional API denylist** | The generic-API fallback supports an opt-in denylist (`MOD_EVENT_AGENT_API_DENYLIST="cmd1,cmd2,..."` or the matching XML `<param name="api_denylist">`). **Default is empty** — the module forwards every API verb to FreeSWITCH, behaving as a transparent ESL-over-NATS bridge. The list is a defense-in-depth knob for sandbox / compliance deployments; primary authorization belongs at the NATS layer (token, NKey, subject ACL, JWT claims), not here. |
| **Subject-prefix sanitisation** | The configurable `subject_prefix` is capped at 64 chars and any character outside `[a-zA-Z0-9._-]` is replaced with `_` — eliminates a CR/LF wire-protocol injection surface from a hostile config. |
| **Subscription cap** | The driver refuses to register more than 32 simultaneous subscriptions (the module legitimately needs ≤ 2). |

### 3. Forensic audit trail

Every command attempt emits a single INFO line of the shape

```
[mod_event_agent] audit=command cmd=<name> subject=<subj> result=ok|err
                  async=yes|no payload_bytes=<size>
```

Payload contents are deliberately **not** logged — `event_context`
may carry PII. Operators grep `audit=command` for the trail.

See `INSTALL.md` for the operator runbook including how to verify
each guard with the bundled tests.

---

## 🚦 Quick Start

The fastest path to a working module in a dev stack:

```bash
# 1. Build the FS+module image (the Dockerfile bundles libnats + the .so)
docker build -t freeswitch-events-agent:latest -f Dockerfile.freeswitch .

# 2. Run it pointing at any NATS broker on your network
docker run -d --name fs \
    -e MOD_EVENT_AGENT_URL=nats://nats:4222 \
    -e MOD_EVENT_AGENT_TOKEN=$NATS_TOKEN \
    -e MOD_EVENT_AGENT_NODE_ID=fs-prod-01 \
    freeswitch-events-agent:latest

# 3. Smoke-check from any host that can reach the same broker
nats --server nats://nats:4222 \
     req freeswitch.api '{"command":"agent.status"}' --timeout 3s
```

For the canonical FreeSWITCH-native install (drop-in inside
`freeswitch/src/mod/applications/`, autotools build), or for the
standalone Makefile path used during development, see **[INSTALL.md](INSTALL.md)**.

### Logging note

`mod_event_agent` writes straight through `switch_log_printf`, so
verbosity follows the regular FreeSWITCH logging knobs:

```
fs_cli -x "log debug"
```

## 📁 Project Structure

```
mod_events_agent/
├── src/
│   ├── mod_event_agent.c          # Module entry point
│   ├── mod_event_agent.h          # Main header
│   ├── core/
│   │   └── config.c               # XML + env config parser
│   ├── events/
│   │   ├── adapter.c              # FS event subscription & publishing
│   │   └── serializer.c           # JSON serialization
│   ├── dialplan/
│   │   ├── manager.c              # XML binding & park mode
│   │   └── commands.c             # Dialplan-control commands
│   ├── commands/
│   │   ├── handler.c              # Command dispatcher (size cap, audit log)
│   │   ├── core.c                 # Request envelope helpers
│   │   ├── api.c                  # Generic API execution + optional denylist
│   │   ├── call.c                 # Originate / hangup
│   │   └── status.c               # agent.status
│   ├── validation/
│   │   ├── validation.c           # Typed payload validators
│   │   └── validation.h           # v_len / v_range macros
│   └── drivers/
│       ├── interface.h            # Driver interface
│       └── nats.c                 # NATS driver (subscription cap, leak fixes)
│
├── tests/
│   ├── Makefile                   # Builds the four test binaries
│   ├── run.sh                     # Orchestrator (build + smoke + run)
│   └── src/
│       ├── show_modules_test.c    # Legacy quick-check
│       ├── test_size_limit.c      # Verifies the 64KB cap
│       ├── test_denylist.c        # Opt-in denylist: SKIP unless env set, else asserts denial
│       └── test_validation.c      # 11 invalid-payload cases
│
├── docs/
│   ├── API.md                     # Complete API reference
│   ├── DIALPLAN_CONTROL.md        # Dialplan control guide
│   └── ROADMAP.md                 # Driver development roadmap
│
├── m4/
│   └── ax_lib_nats.m4             # Autoconf detection for libnats
├── autoload_configs/
│   └── mod_event_agent.conf.xml   # Config template (env vars preferred)
│
├── INSTALL.md                     # Operator install guide (3 paths)
├── Makefile                        # Standalone build (custom)
├── Makefile.am                     # In-tree FreeSWITCH build (autotools)
├── configure.ac.snippet            # FS configure.ac integration recipe
├── Dockerfile                      # Module-only image (build artifacts)
├── Dockerfile.freeswitch           # FS + module pre-baked image
├── docker-compose.dev.yaml         # Local dev stack (FS + NATS)
└── install.sh                      # In-container helper installer
```

---

## 🎯 Available Commands

| Command | Description | Reply |
|---------|-------------|-------|
| `originate` | Create outbound call with endpoint/extension/context fields | ✅ Yes |
| `hangup` | Terminate a UUID with optional `cause` | ✅ Yes |
| `agent.status` | Module stats (version + metrics) | ✅ Yes |
| `dialplan.enable` | Enable park mode | ✅ Yes |
| `dialplan.disable` | Disable park mode | ✅ Yes |
| `dialplan.audio` | Configure park audio (`mode`, optional `music_class`) | ✅ Yes |
| `dialplan.autoanswer` | Toggle auto-answer for parked calls | ✅ Yes |
| `dialplan.status` | Snapshot of park manager state | ✅ Yes |

Any other `command` value is passed directly to the native FreeSWITCH API, so `"command":"status"`, `"command":"show"`, `"command":"uuid_bridge"`, etc., keep working without extra configuration.

> ℹ️ Bridge, transfer, and media manipulation go through the native API fallback with commands such as `uuid_bridge`, `uuid_transfer`, `uuid_broadcast`, etc.

### Async Delivery

Add `"async": true` to any payload when you do not need a reply. The server still executes the handler, updates metrics, and logs errors, but the request immediately returns on the client side.

### Events (Pub/Sub)

| Subject Pattern | Description |
|-----------------|-------------|
| `freeswitch.events.channel.*` | Channel lifecycle events |
| `freeswitch.events.call.*` | Call-related events |
| `freeswitch.events.custom.*` | Custom events |

**Full API documentation**: [docs/API.md](docs/API.md)

---

## 🔧 Configuration Options

### Basic Settings

```xml
<param name="driver" value="nats"/>              <!-- Driver: nats (others in roadmap) -->
<param name="url" value="nats://host:4222"/>     <!-- Broker connection URL -->
<param name="subject_prefix" value="freeswitch"/> <!-- Subject prefix (freeswitch.api, freeswitch.node.*) -->
<param name="node-id" value="fs-node-01"/>       <!-- Unique node identifier -->
```

### NATS-Specific

## 🚀 Installation

Three install paths are documented in **[INSTALL.md](INSTALL.md)**.
Quick summary:

| Path | When |
|------|------|
| **A. In-tree FreeSWITCH build** | You compile FS from source. Module sits at `freeswitch/src/mod/applications/mod_event_agent/` and is built by FS's autotools — same pattern as `mod_skel`, `mod_amqp`, etc. Requires `Makefile.am` (in this repo) plus the three configure.ac edits documented in `configure.ac.snippet`. |
| **B. Standalone build** | You have FS pre-installed with headers available. `make && sudo make install` against the included `Makefile`. Useful for local iteration. |
| **C. Docker image** | Use `Dockerfile.freeswitch` to bake the module into a complete FS image. Used in `docker-compose.dev.yaml` for the dev stack. |

### Build requirements

- FreeSWITCH 1.10+ (headers available, either via FS source tree or
  installed at `/usr/local/freeswitch/include/freeswitch`)
- Linux/Unix
- gcc, make, pkg-config
- `libcjson-dev` ≥ 1.7
- `libssl-dev`
- `libnats` headers + lib (system install OR the bundled
  `lib/nats/libnats_static.a` for the standalone path)

### Loading order

The module registers a single XML binding (the dynamic dialplan) at
load. Register it in `modules.conf.xml` after the standard
endpoint/codec modules so any park-mode interception only takes
effect once the rest of the dialplan is wired:

```xml
<load module="mod_dialplan_xml"/>
...
<load module="mod_event_agent"/>
```

---

## ⚙️ Configuration

Two layers, in precedence order:

### Layer 1 — Environment variables (recommended for production)

```bash
export MOD_EVENT_AGENT_URL=nats://nats:4222
export MOD_EVENT_AGENT_TOKEN=<auth-token>      # OR …
export MOD_EVENT_AGENT_NKEY_SEED=<nkey-seed>
export MOD_EVENT_AGENT_NODE_ID=fs-prod-01
```

Env wins over XML and is the only path that should carry secrets.

### Layer 2 — `autoload_configs/mod_event_agent.conf.xml`

Used for non-secret fields (event filters, subject prefix) and as a
fallback for the URL/node id when env is absent. The reference file
shipped at `autoload_configs/mod_event_agent.conf.xml` documents
every supported parameter inline.

```xml
<configuration name="event_agent.conf">
  <settings>
    <param name="driver" value="nats"/>
    <param name="url" value="nats://$${nats_host}:$${nats_port}"/>

    <!-- Leave token / nkey_seed BLANK in production. The module
         picks them up from env vars; a non-empty value here logs a
         DEPRECATED warning at module load. -->
    <param name="token" value=""/>
    <param name="nkey_seed" value=""/>

    <!-- Event filtering -->
    <param name="publish_all_events" value="true"/>
    <param name="exclude" value="DTMF,HEARTBEAT"/>

    <param name="subject_prefix" value="freeswitch"/>
    <param name="node_id" value="$${agent_node_id}"/>
  </settings>
</configuration>
```

> Reconnect / timeout knobs are baked into the driver
> (`MaxReconnect=60`, `ReconnectWait=1s`, `ReconnectBufSize=8MB`) and
> are not currently configurable. Open an issue if you need them
> exposed.

### Multi-Node Configuration

For FreeSWITCH clusters, assign a unique `node-id` to each node:

```xml
<!-- Node 1 -->
<param name="node-id" value="fs-node-01"/>

<!-- Node 2 -->
<param name="node-id" value="fs-node-02"/>
```

Clients can filter responses by `node_id` in the JSON response.

---

## 🎯 Quick Usage

### Install NATS Server

```bash
# Docker (~10MB image)
docker run -d --name nats -p 4222:4222 nats:latest

# Or direct binary (https://nats.io/download/)
wget https://github.com/nats-io/nats-server/releases/download/v2.10.7/nats-server-v2.10.7-linux-amd64.tar.gz
tar xzf nats-server-*.tar.gz
./nats-server
```

### Compile Example Clients

Build the helper binaries under `tests/` with the provided Makefile, then run:

```bash
cd tests

# service_a client: Sends commands and receives responses
LD_LIBRARY_PATH=../lib/nats ./bin/service_a_nats '{"command":"status"}'

# service_b client: Processes commands (simulation)
LD_LIBRARY_PATH=../lib/nats ./bin/service_b_nats

# simple client: Multi-mode (pub/req/server)
LD_LIBRARY_PATH=../lib/nats ./bin/simple_test req freeswitch.api '{"command":"version"}'
```

### Command Examples

```bash
# System status
./bin/show_modules_test 
# → {"success":true,"message":"API command executed","data":"(list modules)","timestamp":...,"node_id":"fs-node-01"}

```

See [docs/API.md](docs/API.md) for complete command documentation.

---

## 📊 Comparison vs ESL

| Aspect | mod_event_agent + NATS | ESL (Event Socket Library) |
|---------|------------------------|----------------------------|
| **Protocol** | NATS (text, open standard) | Proprietary binary |
| **Dependencies** | None (static lib) | libesl + ~7MB deps |
| **Debugging** | `telnet`, `nats` CLI, any tool | Specific ESL client |
| **Languages** | Any with NATS client | Specific bindings (Node, Python, etc.) |
| **Latency** | 0.5-1ms (local) | 2-5ms |
| **Throughput** | ~10,000 req/s | ~1,000 req/s |
| **Scalability** | Native (NATS clustering) | Requires proxy/balancer |
| **Event Streaming** | Native Pub/Sub | Socket connection 1:1 |
| **Multi-Node** | Yes (node filtering) | Multiple connections |

---

### Validated Performance (Production)

- ✅ **100,000 requests**: 100% success rate
- ✅ **50 concurrent clients**: No packet loss
- ✅ **Production**: 1,055 requests, 99.7% success
- ✅ **Latency**: <100ms (average <1ms local)

### 1. Distributed Microservices
```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  Service A  │────▶│    NATS     │◀────│  Service B  │
│  (Node.js)  │     │   Broker    │     │   (Python)  │
└─────────────┘     └──────┬──────┘     └─────────────┘
                           │
                    ┌──────▼──────┐
                    │ FreeSWITCH  │
                    │ mod_event   │
                    │   _agent    │
                    └─────────────┘

- Multiple services control FreeSWITCH without direct dependencies
- Horizontal broker scalability
- Heterogeneous languages (Node, Python, Go, Java, etc.)
```

### 2. Event-Driven Architecture
```
FreeSWITCH Events → NATS → [
    • Analytics Service (Python)
    • Billing Service (Go)
    • Notification Service (Node.js)
    • CDR Storage (Java)
]

- Real-time event streaming
- Parallel event processing
- Total decoupling between producers and consumers
```

### 3. Distributed Call Center
```
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│ FreeSWITCH 1 │  │ FreeSWITCH 2 │  │ FreeSWITCH 3 │
│ (New York)   │  │ (London)     │  │ (Tokyo)      │
└───────┬──────┘  └───────┬──────┘  └───────┬──────┘
        │                 │                  │
        └─────────────────┴──────────────────┘
                          │
                    ┌─────▼─────┐
                    │   NATS    │
                    │  Cluster  │
                    └─────┬─────┘
                          │
                ┌─────────┴─────────┐
        ┌───────▼────────┐  ┌──────▼───────┐
        │  Control Panel │  │   Monitor    │
        │   (Web UI)     │  │  Dashboard   │
        └────────────────┘  └──────────────┘

- Centralized control of multiple FreeSWITCH nodes
- Geographic load balancing
- Real-time global monitoring
```
 
---

## 🧪 Testing

The repo ships an integration test suite under `tests/` that talks
to a running module via real NATS, the same way a production client
would.

```bash
# 1. Bring up the dev stack (FS + NATS in docker)
docker compose -f docker-compose.dev.yaml up -d

# 2. Build + run the suite. run.sh probes the broker and the module
# before each test so failures are unambiguous.
cd tests
make
MOD_EVENT_AGENT_URL=nats://localhost:7001 \
MOD_EVENT_AGENT_NODE_ID=fs-audit \
./run.sh
```

The four test binaries:

| Binary | Asserts |
|--------|---------|
| `test_size_limit`  | The 64KB payload cap rejects oversized requests with the `Payload too large` message. |
| `test_denylist`    | If `MOD_EVENT_AGENT_API_DENYLIST` is set, every verb in it is blocked with the canonical message. If unset, the test SKIPs after sanity-checking that `uptime`/`version`/`status` still go through (transparent-bridge mode). |
| `test_validation`  | 11 invalid-payload cases (parse errors, missing fields, wrong types, enum violations) each return the expected error. |
| `show_modules_test`| Legacy quick-check; sends `show modules` and prints the response. |

Last verified run against `freeswitch-events-agent` Docker image:

```
test_size_limit  : ALL PASS  (3/3 cases)
test_denylist    : SKIP (transparent-bridge mode)  ─ or  ALL PASS (N/N denied verbs) when env set
test_validation  : ALL PASS  (11/11 cases)
```

CI consumers should `make check` from `tests/` — `run.sh` exits
non-zero on any failure.

### Coverage map (honest)

What the integration suite verifies today vs the original roadmap:

| Surface | Status | Where |
|---------|--------|-------|
| Generic API commands (`status`, `version`, `uptime`) | ✅ Verified | `test_denylist.c` (3 allowed verbs) |
| Generic API command `global_getvar` | ⚠️ Not in suite | Trivial to add — same shape as `version` |
| Typed validators (`originate`, `hangup`, `dialplan.*`) | ✅ Verified | `test_validation.c` (11 cases) |
| Successful `originate` round-trip (real call) | ❌ Not in suite | Needs a SIP endpoint to dial; deferred |
| Async fire-and-forget delivery | ⚠️ Code path tested implicitly | No explicit assertion |
| Event streaming (channel.* events) | ❌ Not in suite | Needs a sourced event in FS — deferred |
| Performance benchmarks (throughput, latency) | ❌ Not in suite | Numbers in this README come from earlier ad-hoc runs |
| Multi-node filtering (broadcast vs direct) | ⚠️ Direct subject used everywhere | Broadcast `<prefix>.api` filter not asserted |
| Concurrent clients (race conditions) | ❌ Not in suite | Single-threaded smoke today |
| **Hardening guards** (size cap, denylist, validators) | ✅ Verified | New: `test_size_limit.c`, `test_denylist.c`, `test_validation.c` |
| **Cluster failover** (multi-URL via `SetServers`) | ✅ Smoke verified | Module loads + responds when `MOD_EVENT_AGENT_URL` carries multiple URLs |

### Cluster failover

The driver detects a comma in `MOD_EVENT_AGENT_URL` and switches from
`natsOptions_SetURL` to `natsOptions_SetServers`. libnats then handles
connect-and-failover across the list automatically.

```bash
# Single broker (the common case)
MOD_EVENT_AGENT_URL=nats://nats:4222

# Cluster (libnats picks one and falls over to the others on disconnect)
MOD_EVENT_AGENT_URL=nats://nats-1:4222,nats://nats-2:4222,nats://nats-3:4222
```

- The module accepts up to 16 servers in the list.
- Whitespace around commas is trimmed (so `"a, b"` works).
- One module log line at INFO confirms how many servers libnats was
  configured with: `[mod_event_agent] NATS configured with N servers
  (cluster failover)`.

---

## 🛠️ Driver Development

See [docs/ROADMAP.md](docs/ROADMAP.md) for details on implementing new drivers.

### Implementing a New Driver

1. **Copy template**: `cp src/drivers/driver_nats.c src/drivers/driver_mydriver.c`
2. **Implement interface**: Complete all `event_driver_t` methods
3. **Add to Makefile**: Add `WITH_MYDRIVER=yes` flag
4. **Testing**: Create tests in `tests/`
5. **Documentation**: Update docs/ROADMAP.md

### Driver Interface

```c
typedef struct event_driver {
    // Initialization
    switch_status_t (*init)(const char *url, const char *node_id);
    
    // Cleanup
    void (*shutdown)(void);
    
    // Commands (request-reply)
    switch_status_t (*subscribe_commands)(command_callback_t callback);
    switch_status_t (*send_command_response)(const char *reply_subject, 
                                             const char *json_response);
    
    // Events (pub/sub)
    switch_status_t (*publish_event)(const char *subject, 
                                     const char *json_payload);
    
    // Health check
    switch_bool_t (*is_connected)(void);
} event_driver_t;
```

---

## 📚 Documentation

- **[docs/API.md](docs/API.md)**: Complete API reference
  - JSON payload formats
  - Available commands (sync/async)
  - Response codes
  - Usage examples

- **[docs/ROADMAP.md](docs/ROADMAP.md)**: Drivers roadmap
  - Current status of each driver
  - Implementation guides
  - Contributions

- **[example/README.md](example/README.md)**: Web interface example
  - Vanilla JS implementation
  - Node.js native server
  - Real-time call control

---

## 🙏 Credits

- **FreeSWITCH**: https://freeswitch.org/
- **NATS**: https://nats.io/
- **NATS C Client**: https://github.com/nats-io/nats.c

---

## 📞 Support

- **Issues**: https://github.com/zenozaga/mod_events_agent/issues
- **Documentation**: [docs/](docs/)
- **Web Interface**: [example/](example/)

---

## 📄 License

MIT License - See [LICENSE](LICENSE) for details.
