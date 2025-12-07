# Dynamic Dialplan Control

mod_event_agent includes a dynamic dialplan manager that allows you to control inbound call behavior via NATS commands without restarting FreeSWITCH or editing XML files.

## Features

- **Park Mode**: Intercept all inbound calls and park them until you decide what to do
- **Audio Modes**: Choose between silence, ringback tone, or music on hold
- **Auto-Answer**: Optional automatic answering of calls
- **Real-time Control**: Change behavior instantly via NATS commands
- **Statistics**: Track intercepted and parked calls

## Architecture

The dialplan manager uses FreeSWITCH's XML binding API to dynamically inject dialplan rules. When enabled, it intercepts dialplan lookups and returns a custom park extension.

## Configuration Modes

### Dialplan Modes

- **DISABLED** (default): Normal dialplan processing, no interception
- **PARK**: All inbound calls are intercepted and parked

### Audio Modes

- **SILENCE**: No audio, caller hears silence
- **RINGBACK**: Caller hears ringback tone (ring-ring sound)
- **MUSIC**: Caller hears music on hold (configurable MOH class)

### Auto-Answer

- **Disabled** (default): Call is not answered, remains in early media state
- **Enabled**: Call is automatically answered before parking

## NATS Commands

All commands use the prefix `fs.cmd.dialplan.*`

### Enable Park Mode

**Subject:** `fs.cmd.dialplan.enable`
**Payload:** None

Enables park mode. All inbound calls will be intercepted and parked.

**Example:**
```bash
nats pub fs.cmd.dialplan.enable ""
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

**Subject:** `fs.cmd.dialplan.disable`
**Payload:** None

Disables park mode. Normal dialplan processing resumes.

**Example:**
```bash
nats pub fs.cmd.dialplan.disable ""
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

**Subject:** `fs.cmd.dialplan.audio`
**Payload:**
```json
{
  "mode": "silence|ringback|music",
  "music_class": "moh"  // optional, only for music mode
}
```

Sets the audio mode for parked calls.

**Example - Ringback:**
```bash
nats pub fs.cmd.dialplan.audio '{"mode":"ringback"}'
```

**Example - Music:**
```bash
nats pub fs.cmd.dialplan.audio '{"mode":"music","music_class":"moh"}'
```

**Response:**
```json
{
  "status": "success",
  "message": "Audio mode updated",
  "mode": "ringback"
}
```

### Set Auto-Answer

**Subject:** `fs.cmd.dialplan.autoanswer`
**Payload:**
```json
{
  "enabled": true|false
}
```

Enables or disables auto-answer for parked calls.

**Example:**
```bash
nats pub fs.cmd.dialplan.autoanswer '{"enabled":true}'
```

**Response:**
```json
{
  "status": "success",
  "message": "Auto-answer updated",
  "enabled": true
}
```

### Get Status

**Subject:** `fs.cmd.dialplan.status`
**Payload:** None

Returns current dialplan configuration and statistics.

**Example:**
```bash
nats pub fs.cmd.dialplan.status ""
```

**Response:**
```json
{
  "status": "success",
  "info": "Dialplan Manager Status:\n  Mode: PARK\n  Audio Mode: RINGBACK\n  Auto Answer: NO\n  Context: default\n  Music Class: moh\n  Calls Intercepted: 42\n  Calls Parked: 42\n"
}
```

## Python Client

A complete Python client is provided for easy dialplan control.

### Installation

```bash
pip install nats-py
```

### Interactive Mode

```bash
python examples/dialplan_controller.py --nats nats://localhost:4222
```

This opens an interactive menu where you can:
- Enable/disable park mode
- Change audio modes
- Configure auto-answer
- View status
- Use quick setup presets

### Command Line Mode

**Enable park with ringback:**
```bash
python examples/dialplan_controller.py \
  --mode enable \
  --audio ringback
```

**Enable park with music and auto-answer:**
```bash
python examples/dialplan_controller.py \
  --mode enable \
  --audio music \
  --auto-answer
```

**Disable park:**
```bash
python examples/dialplan_controller.py --mode disable
```

**Get status:**
```bash
python examples/dialplan_controller.py --mode status
```

## Use Cases

### 1. Call Queue with Custom Routing

Enable park mode and let your application decide where to route each call:

```python
# Enable park with music
await controller.enable_park()
await controller.set_audio_mode("music", "moh")
await controller.set_auto_answer(True)

# Your app receives CHANNEL_PARK event via NATS
# Analyze caller, time of day, queue status, etc.
# Then route the call using uuid_transfer or uuid_bridge
```

### 2. Business Hours Control

Automatically park calls outside business hours:

```python
import datetime

def is_business_hours():
    now = datetime.datetime.now()
    return 9 <= now.hour < 18 and now.weekday() < 5

if is_business_hours():
    await controller.disable_park()  # Normal routing
else:
    await controller.enable_park()   # Park for review
    await controller.set_audio_mode("music")
```

### 3. Emergency Mode

During emergencies, park all calls with a special message:

```python
# Enable park with custom message
await controller.enable_park()
await controller.set_audio_mode("music", "emergency_message")
await controller.set_auto_answer(True)
```

### 4. VIP Detection

Park all calls but with different audio for VIPs:

```python
# Listen for CHANNEL_PARK events
# Check if caller is VIP
# If VIP, play special music: uuid_broadcast <uuid> moh://vip_moh
# If not VIP, keep standard music
```

### 5. Agent Availability

Park calls when no agents available:

```python
agents_available = check_agent_count()

if agents_available > 0:
    await controller.disable_park()  # Route normally
else:
    await controller.enable_park()   # Park until agent available
    await controller.set_audio_mode("music")
```

## Integration with Call Control

Combine with call control commands for complete call management:

```python
# 1. Enable park mode
await controller.enable_park()
await controller.set_audio_mode("ringback")

# 2. Listen for CHANNEL_PARK events on NATS
# Subject: freeswitch.events.CHANNEL_PARK

# 3. When call parks, you have the UUID
# Now you can:

# Answer the call
nats.publish("fs.cmd.call.answer", json.dumps({"uuid": uuid}))

# Bridge to destination
nats.publish("fs.cmd.call.bridge", json.dumps({
    "uuid": uuid,
    "destination": "sofia/gateway/trunk/5551234"
}))

# Transfer to extension
nats.publish("fs.cmd.call.transfer", json.dumps({
    "uuid": uuid,
    "destination": "1000"
}))

# Hangup
nats.publish("fs.cmd.call.hangup", json.dumps({"uuid": uuid}))
```

## Event Flow

```
1. Inbound call arrives
   ↓
2. FreeSWITCH looks up dialplan
   ↓
3. mod_event_agent intercepts (if park mode enabled)
   ↓
4. Returns dynamic park extension
   ↓
5. Call executes: ring_ready + park
   ↓
6. CHANNEL_PARK event published to NATS
   ↓
7. Your application receives event
   ↓
8. Your application decides what to do
   ↓
9. Send bridge/transfer/hangup command via NATS
```

## Performance

The dialplan manager is highly optimized:
- **Zero disk I/O**: No XML file parsing
- **Fast lookup**: Direct function call, no external HTTP requests
- **Low latency**: < 1ms to generate dialplan XML
- **Thread-safe**: Uses mutex for concurrent calls
- **Minimal memory**: Reuses pool allocations

## Logging

The dialplan manager logs all activity:

```
[INFO] Dialplan Manager initialized (mode=disabled)
[INFO] Dialplan Manager: mode changed to PARK
[INFO] Dialplan Manager: audio mode changed to RINGBACK
[INFO] Event Agent: Intercepted call, mode=park, audio=ringback, auto_answer=no
```

## Troubleshooting

### Park mode not working

1. Check dialplan manager is initialized:
   ```bash
   fs_cli -x "module list" | grep event_agent
   ```

2. Verify park mode is enabled:
   ```bash
   python examples/dialplan_controller.py --mode status
   ```

3. Check FreeSWITCH logs:
   ```bash
   tail -f /var/log/freeswitch/freeswitch.log | grep "Event Agent"
   ```

### Calls not being intercepted

1. Verify dialplan context matches:
   - Default context is "default"
   - Check your inbound route context

2. Test with a simple call:
   ```bash
   originate sofia/gateway/trunk/5551234 &park
   ```

### Audio not playing

1. For music mode, verify MOH class exists:
   ```bash
   fs_cli -x "show file moh"
   ```

2. Check audio mode:
   ```bash
   python examples/dialplan_controller.py --mode status
   ```

## Comparison with Static XML

### Static XML (traditional)
- ❌ Requires file editing
- ❌ Needs FreeSWITCH reload
- ❌ Manual configuration
- ✅ Simple for static scenarios

### Dynamic Dialplan Manager
- ✅ Real-time updates via NATS
- ✅ No FreeSWITCH reload needed
- ✅ Programmatic control
- ✅ Perfect for dynamic scenarios
- ✅ Easy testing and experimentation

## Security Considerations

1. **NATS Security**: Use authentication and TLS for production
2. **Command Validation**: Manager validates all inputs
3. **Access Control**: Restrict who can send dialplan commands
4. **Audit Logging**: All changes are logged with timestamps

## Future Enhancements

Potential additions:
- Per-number park rules
- Scheduled park mode changes
- Custom audio files per call
- Conditional park based on caller ID
- Integration with external CRM systems
