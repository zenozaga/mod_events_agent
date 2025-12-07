# Docker Images

## Dockerfile

Build environment for compiling `mod_event_agent.so`.

### Build
```bash
docker build --no-cache -t mod_events_agent:builder .
```

### Features
- Based on `debian:bookworm-slim`
- Installs NATS C Client 3.13.0 from source
- Downloads FreeSWITCH headers from GitHub
- Compiles module with NATS driver support
- Produces `mod_event_agent.so` shared library

### Usage
```bash
docker run --rm -v $(pwd):/work mod_events_agent:builder make DRIVER=nats
```

## Dockerfile.freeswitch

Production-ready FreeSWITCH image with `mod_event_agent` pre-integrated.

### Build
```bash
docker build --no-cache -t freeswitch-events-agent:latest -f Dockerfile.freeswitch .
```

### Features
- Multi-stage build for minimal image size
- Stage 1: Compiles `mod_event_agent.so` using Dockerfile base
- Stage 2: Uses `drachtio/drachtio-freeswitch-mrf:latest` as base
- Pre-installs NATS C Client libraries
- Copies compiled module to `/usr/local/freeswitch/mod/`
- Includes configuration file `mod_event_agent.conf.xml`
- Auto-loads module on FreeSWITCH startup

### Exposed Ports
- `5060/tcp` - SIP (TCP)
- `5060/udp` - SIP (UDP)
- `5080/tcp` - WebSocket SIP (TCP)
- `5080/udp` - WebSocket SIP (UDP)
- `8021/tcp` - Event Socket Layer

### Usage
```bash
docker run -d \
  --name freeswitch-events \
  -p 5060:5060/tcp \
  -p 5060:5060/udp \
  -p 5080:5080/tcp \
  -p 5080:5080/udp \
  -p 8021:8021/tcp \
  -e NATS_URL="nats://nats-server:4222" \
  freeswitch-events-agent:latest
```

### Environment Variables
All configuration is managed via `/usr/local/freeswitch/conf/autoload_configs/mod_event_agent.conf.xml`:
- `nats_url` - NATS server connection URL
- `subject_prefix` - Subject prefix for NATS messages
- `node_id` - Unique identifier for this FreeSWITCH instance

## docker-compose.dev.yaml

Development environment with FreeSWITCH + NATS.

### Usage
```bash
make docker-up
```

### Services
- `freeswitch` - FreeSWITCH 1.10 with mounted source code
- `nats` - NATS Server alpine3.22

### Volumes
- `freeswitch_sounds` - Persistent sound files
- `freeswitch_recordings` - Call recordings
- `postgres_data` - PostgreSQL data (if using database features)
