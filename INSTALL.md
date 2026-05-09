# Installing `mod_event_agent`

Three install paths are supported. They all produce the same
`mod_event_agent.so` and the same runtime behaviour. Pick the one
that matches your environment.

| Path | When to use |
|------|-------------|
| **A. In-tree FreeSWITCH build** (autotools) | You compile FreeSWITCH from source. This is the most production-like; the module is built and installed alongside FS. |
| **B. Standalone build** (custom Makefile) | You have a pre-installed FreeSWITCH with headers available, want quick iteration, or are testing locally. |
| **C. Docker image** (Dockerfile / Dockerfile.freeswitch) | You ship FS as a container; the module is baked in or injected via volume mount. |

---

## A. In-tree FreeSWITCH build

The canonical FreeSWITCH-native pattern. Mirrors `mod_skel`,
`mod_amqp`, etc.

```bash
# 1. Drop the module into the FS source tree.
cp -r mod_event_agent freeswitch/src/mod/applications/mod_event_agent

# 2. One-time edit of FS configure.ac (see configure.ac.snippet for
#    the exact lines). Three additions:
#       - m4_include of m4/ax_lib_nats.m4
#       - PKG_CHECK_MODULES for libcjson + AX_LIB_NATS
#       - Add the module's Makefile to AC_CONFIG_FILES
#       - Add `applications/mod_event_agent` to build/modules.conf.in

# 3. Bootstrap + configure + build.
cd freeswitch
./bootstrap.sh
./configure --enable-core-pgsql-support  # or your usual flags
make -j$(nproc)
sudo make install

# 4. Verify.
fs_cli -x "module_exists mod_event_agent"
```

After step 3, `mod_event_agent.so` is at
`/usr/local/freeswitch/mod/mod_event_agent.so` automatically. Any
subsequent FS rebuild rebuilds the module without manual steps.

**Required deps on the build host:**
- `libcjson-dev` ≥ 1.7
- `libnats` headers + library (https://github.com/nats-io/nats.c)
- `libssl-dev`
- The standard FreeSWITCH build deps (gcc, make, libtool, autoconf,
  pkg-config, etc.)

---

## B. Standalone build

For development against an installed FreeSWITCH (the usual flow when
you do not own the FS build system).

```bash
# 1. Make sure FS headers are available. The Makefile defaults look at
#    /tmp/freeswitch_headers — adjust if your FS exposes them at a
#    different path:
export FREESWITCH_INCLUDE_DIR=/usr/local/freeswitch/include/freeswitch

# 2. Build.
cd mod_event_agent
make clean
make

# 3. Install the .so + the default config.
sudo install -D -m 755 mod_event_agent.so \
    /usr/local/freeswitch/mod/mod_event_agent.so
sudo install -D -m 644 autoload_configs/mod_event_agent.conf.xml \
    /usr/local/freeswitch/conf/autoload_configs/mod_event_agent.conf.xml

# 4. Add to modules.conf.xml so it loads on FS start.
#    (or load on demand: fs_cli -x "load mod_event_agent")

# 5. Reload + verify.
fs_cli -x "reload mod_event_agent"
fs_cli -x "module_exists mod_event_agent"
```

**Required deps on the build host:**
- `libcjson-dev`
- `libnats` static lib (bundled at `lib/nats/libnats_static.a`) or
  shared (`lib/nats/libnats.so`)
- `libssl-dev`
- `gcc`, `make`, `pkg-config`

The bundled NATS static library is what ships with this repo so the
standalone path works on machines without the system-wide `libnats`
package. The Makefile auto-detects which it should use:

| Detected | Result |
|----------|--------|
| `/usr/local/lib/libnats.so` exists | Link against system shared lib |
| `./lib/nats/libnats.so` exists | Link against bundled shared lib (rpath set) |
| neither | Statically link `./lib/nats/libnats_static.a` |

---

## C. Docker image

Two Dockerfiles ship with this repo:

| Dockerfile | What it builds |
|------------|----------------|
| `Dockerfile` | A minimal builder + runner that produces `mod_event_agent.so` and the bundled `libnats.so`. Useful as an intermediate stage. |
| `Dockerfile.freeswitch` | A complete FreeSWITCH image with `mod_event_agent` pre-loaded, ready to run. |

```bash
# Build the FS+module image
docker build -t freeswitch-events-agent:latest -f Dockerfile.freeswitch .

# Run it against your NATS broker
docker run -d --name fs \
    --network <your-internal-net> \
    -e MOD_EVENT_AGENT_URL=nats://nats:4222 \
    -e MOD_EVENT_AGENT_TOKEN=$NATS_TOKEN \
    -e MOD_EVENT_AGENT_NODE_ID=fs-prod-01 \
    freeswitch-events-agent:latest
```

For a docker-compose stack, see `docker-compose.dev.yaml` for a
working dev setup with FS + NATS containers wired together.

---

## Configuration via environment variables

Secrets must come from the environment, never the XML config. The
following variables override (or replace) values in
`autoload_configs/mod_event_agent.conf.xml`:

| Env var | Purpose | XML fallback (deprecated) |
|---------|---------|---------------------------|
| `MOD_EVENT_AGENT_URL` | NATS broker URL — single, or comma-separated list for cluster failover | `<param name="url">` |
| `MOD_EVENT_AGENT_TOKEN` | NATS auth token (recommended on any non-localhost broker) | `<param name="token">` |
| `MOD_EVENT_AGENT_NKEY_SEED` | NATS NKey seed (alternative to token) | `<param name="nkey_seed">` |
| `MOD_EVENT_AGENT_NODE_ID` | Unique identifier of this FS node | `<param name="node_id">` |

If a variable AND its XML counterpart are both set, the env wins and
the XML value emits a `DEPRECATED` warning at module load time. Tokens
and NKey seeds in the XML log a louder warning telling the operator
to migrate.

#### Cluster failover via multi-URL

The driver detects a comma in `MOD_EVENT_AGENT_URL` and switches from
`natsOptions_SetURL` to `natsOptions_SetServers`. libnats then handles
connect-and-failover across the URL list automatically — no extra
client logic needed.

```bash
# Single broker (most common)
MOD_EVENT_AGENT_URL=nats://nats:4222

# 3-node NATS cluster — the client tries each in order
MOD_EVENT_AGENT_URL=nats://nats-1:4222,nats://nats-2:4222,nats://nats-3:4222
```

- Up to 16 URLs accepted (`NATS_DRIVER_MAX_SERVERS` in
  `src/drivers/nats.c`).
- Whitespace around commas is trimmed.
- Confirmation log at INFO on startup:
  `[mod_event_agent] NATS configured with N servers (cluster failover)`.

---

## Verifying installation

Three quick checks, in order:

### 1. The module loaded

```bash
fs_cli -x "module_exists mod_event_agent"
# expected: true
```

### 2. The module connected to NATS

```bash
nats --server $MOD_EVENT_AGENT_URL req freeswitch.api '{"command":"agent.status"}' --timeout 3s
```
Expected payload:
```json
{
  "success": true,
  "message": "Module status",
  "node_id": "...",
  "data": {
    "version": "2.0.0",
    "stats": { "requests_received": 1, "requests_success": 0, "requests_failed": 0 }
  }
}
```

### 3. The hardening guards are alive

The denylist must reject `shutdown`:

```bash
nats --server $MOD_EVENT_AGENT_URL req freeswitch.api '{"command":"shutdown"}' --timeout 3s
# expected: { "success": false, "message": "Command is not permitted via the event bus" }
```

The validator must reject malformed payloads:

```bash
nats --server $MOD_EVENT_AGENT_URL req freeswitch.api 'not json' --timeout 3s
# expected: { "success": false, "message": "Invalid JSON payload" }
```

A broader matrix lives at `tests/run.sh` if you want to verify
size limits, validation rules, and the full denylist programmatically.
