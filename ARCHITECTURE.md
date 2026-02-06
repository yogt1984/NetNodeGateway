# ARCHITECTURE.md

## 1. Gap Analysis: REQUIREMENTS.md vs README.md

The REQUIREMENTS.md covers the structural questions posed by the README (UDP
ingestion, TCP control, record/replay, protocol format, validation,
observability, verification strategy). However, several areas remain
unspecified:

| Area | README mentions | REQUIREMENTS covers | Gap |
|------|----------------|---------------------|-----|
| UDP telemetry protocol | Header fields, CRC | FR-1, FR-2 | None |
| TCP control protocol | Length-prefix, commands | FR-3 | None |
| Record / Replay | Record to disk, replay | FR-4 | None |
| Observability / stats | Counters, health | FR-5 | None |
| **Domain object payloads** | msg_type PLOT/TRACK/HEARTBEAT | Not specified | **What goes inside the payload bytes for each msg_type** |
| **Event taxonomy** | "inject loss/reorder/duplication" | Mentioned but not enumerated | **No concrete event catalog with severity/category** |
| **Log format** | "Logs shall be timestamped and structured" | NFR only | **No concrete log schema or example output** |
| **Sensor simulation profiles** | "configurable rate" | General | **No realistic object generation profiles** |
| **Domain realism** | Mentions "industrial and defense-grade" | Generic | **No domain-specific object models** |

This architecture document fills those gaps and defines how to proceed from
requirements to implementation.

---

## 2. Domain Context: Air Defense Sensor Network

The simulation models a **short-range air defense (SHORAD) sensor and effector
network**, representative of systems built by Rheinmetall Oerlikon (Skyshield,
Skyranger, Revolver Gun families). The system tracks airborne objects detected
by radar and electro-optical sensors, evaluates threats, and logs engagement
decisions.

### 2.1 Domain Objects

These are the payload structures carried inside `TelemetryFrame.payload`:

#### TRACK (msg_type = 0x02)

A radar/sensor track representing a detected airborne object.

```
TrackPayload {
    track_id        : u32       // unique track number
    classification  : u8        // see TrackClass enum
    threat_level    : u8        // 0=UNKNOWN, 1=LOW, 2=MEDIUM, 3=HIGH, 4=CRITICAL
    iff_status      : u8        // 0=NO_RESPONSE, 1=FRIEND, 2=FOE, 3=PENDING
    azimuth_mdeg    : i32       // millidegrees, 0..360000
    elevation_mdeg  : i32       // millidegrees, -5000..+900000
    range_m         : u32       // meters
    velocity_mps    : i16       // m/s radial velocity (negative = closing)
    rcs_dbsm        : i16       // radar cross section in 0.01 dBsm
    update_count    : u16       // how many updates this track has received
}
```

TrackClass enum:
```
0x00 = UNKNOWN
0x01 = FIXED_WING
0x02 = ROTARY_WING
0x03 = UAV_SMALL       // Group 1-2 UAS
0x04 = UAV_LARGE       // Group 3-5 UAS
0x05 = MISSILE
0x06 = ROCKET_ARTILLERY // RAM target
0x07 = BIRD            // clutter / false alarm
0x08 = DECOY
```

#### PLOT (msg_type = 0x01)

A raw radar detection (unassociated measurement), before track formation.

```
PlotPayload {
    plot_id         : u32
    azimuth_mdeg    : i32
    elevation_mdeg  : i32
    range_m         : u32
    amplitude_db    : i16       // signal strength in 0.1 dB
    doppler_mps     : i16       // doppler-derived radial velocity
    quality         : u8        // 0..100 detection quality score
}
```

#### HEARTBEAT (msg_type = 0x03)

Subsystem liveness report.

```
HeartbeatPayload {
    subsystem_id    : u16       // which subsystem
    state           : u8        // 0=OK, 1=DEGRADED, 2=ERROR, 3=OFFLINE
    cpu_pct         : u8        // CPU load percentage
    mem_pct         : u8        // memory usage percentage
    uptime_s        : u32       // seconds since last restart
    error_code      : u16       // 0 = no error
}
```

#### ENGAGEMENT_STATUS (msg_type = 0x04) -- extension

Weapon system status event.

```
EngagementPayload {
    weapon_id       : u16
    mode            : u8        // 0=SAFE, 1=ARMED, 2=ENGAGING, 3=CEASE_FIRE
    assigned_track  : u32       // track_id being engaged, 0=none
    rounds_remaining: u16
    barrel_temp_c   : i16       // Celsius
    burst_count     : u16       // total bursts fired this session
}
```

### 2.2 Event Taxonomy

Events are discrete occurrences logged by the gateway. Each event has a
category, severity, and structured payload.

| Event ID | Category | Severity | Description |
|----------|----------|----------|-------------|
| EVT_TRACK_NEW | TRACKING | INFO | New track initiated from plots |
| EVT_TRACK_UPDATE | TRACKING | DEBUG | Existing track updated |
| EVT_TRACK_LOST | TRACKING | WARN | Track dropped (no updates for N cycles) |
| EVT_TRACK_CLASSIFY | TRACKING | INFO | Track classification changed |
| EVT_THREAT_EVAL | THREAT | INFO | Threat level assigned/changed |
| EVT_THREAT_CRITICAL | THREAT | ALARM | Threat level reached CRITICAL |
| EVT_IFF_RESPONSE | IFF | INFO | IFF interrogation result received |
| EVT_IFF_FOE | IFF | ALARM | Target identified as FOE |
| EVT_ENGAGE_START | ENGAGEMENT | WARN | Weapon assigned to track |
| EVT_ENGAGE_CEASE | ENGAGEMENT | INFO | Engagement ceased |
| EVT_WEAPON_STATUS | ENGAGEMENT | INFO | Weapon periodic status |
| EVT_AMMO_LOW | ENGAGEMENT | WARN | Rounds remaining below threshold |
| EVT_SEQ_GAP | NETWORK | WARN | Sequence gap detected on source |
| EVT_SEQ_REORDER | NETWORK | WARN | Out-of-order frame detected |
| EVT_FRAME_MALFORMED | NETWORK | ERROR | Frame failed validation |
| EVT_CRC_FAIL | NETWORK | ERROR | CRC mismatch |
| EVT_SOURCE_ONLINE | NETWORK | INFO | New source started sending |
| EVT_SOURCE_TIMEOUT | NETWORK | WARN | Source stopped sending |
| EVT_HEARTBEAT_OK | HEALTH | DEBUG | Subsystem heartbeat normal |
| EVT_HEARTBEAT_DEGRADE | HEALTH | WARN | Subsystem reported DEGRADED |
| EVT_HEARTBEAT_ERROR | HEALTH | ERROR | Subsystem reported ERROR |
| EVT_CONFIG_CHANGE | CONTROL | INFO | Runtime config changed via TCP |

Severity levels: `DEBUG < INFO < WARN < ALARM < ERROR < FATAL`

---

## 3. Random Object & Event Generation

The sensor simulator shall generate realistic randomized scenarios. This is
critical for testing the full pipeline without real hardware.

### 3.1 Object Generator

The simulator maintains a world model with randomized airborne objects:

```
WorldObject {
    id              : u32
    class           : TrackClass
    spawn_time_s    : f64
    lifetime_s      : f64       // how long object exists
    start_az_deg    : f64       // initial azimuth
    start_el_deg    : f64       // initial elevation
    start_range_m   : f64       // initial range
    speed_mps       : f64       // ground speed
    heading_deg     : f64       // direction of travel
    rcs_dbsm        : f64       // radar cross section
    is_hostile       : bool
    noise_stddev     : f64      // measurement noise sigma
}
```

**Generation profiles:**

| Profile | Objects | Types | Rate (Hz) | Duration |
|---------|---------|-------|-----------|----------|
| `idle` | 0-2 | BIRD, UNKNOWN | 10 | continuous |
| `patrol` | 3-8 | FIXED_WING, ROTARY_WING, UAV_SMALL | 50 | 60s bursts |
| `raid` | 10-30 | UAV_SMALL, MISSILE, ROCKET_ARTILLERY | 100 | 30s |
| `stress` | 50-100 | mixed | 500-5000 | 120s |
| `scripted` | from file | from file | from file | from file |

Each tick the simulator:
1. Advances object positions (linear + noise)
2. Generates PLOTs from visible objects (with detection probability based on RCS and range)
3. Associates PLOTs to TRACKs (simple nearest-neighbor)
4. Emits TRACK frames for associated tracks
5. Periodically emits HEARTBEAT frames for each simulated subsystem
6. Injects configurable faults: packet loss, reorder, duplication, corruption

### 3.2 Event Generator

Events are produced by the gateway as a byproduct of processing:

- **Track lifecycle events** fire when the gateway's track table changes
- **Threat evaluation events** fire when classification or IFF data triggers re-evaluation
- **Network anomaly events** fire on sequence gaps, CRC failures, etc.
- **Engagement events** fire when ENGAGEMENT_STATUS frames arrive or thresholds are crossed

### 3.3 Example Log Output

The gateway produces structured log lines. Example output from a `raid`
scenario:

```
2025-07-15T14:23:01.001Z [INFO ] [TRACKING  ] EVT_TRACK_NEW       src=0x0012 track_id=1041 class=UAV_SMALL az=045.2 el=012.8 range=8200m
2025-07-15T14:23:01.003Z [INFO ] [TRACKING  ] EVT_TRACK_NEW       src=0x0012 track_id=1042 class=MISSILE   az=047.1 el=005.3 range=12400m
2025-07-15T14:23:01.020Z [INFO ] [THREAT    ] EVT_THREAT_EVAL     track_id=1042 threat=HIGH class=MISSILE vel=-320m/s range=12350m
2025-07-15T14:23:01.021Z [INFO ] [IFF       ] EVT_IFF_RESPONSE    track_id=1042 result=FOE
2025-07-15T14:23:01.021Z [ALARM] [IFF       ] EVT_IFF_FOE         track_id=1042 class=MISSILE iff=FOE range=12340m
2025-07-15T14:23:01.022Z [ALARM] [THREAT    ] EVT_THREAT_CRITICAL track_id=1042 threat=CRITICAL class=MISSILE vel=-320m/s range=12330m closing
2025-07-15T14:23:01.050Z [WARN ] [ENGAGEMENT] EVT_ENGAGE_START    weapon_id=3 track_id=1042 mode=ENGAGING rounds=480
2025-07-15T14:23:01.102Z [DEBUG] [TRACKING  ] EVT_TRACK_UPDATE    src=0x0012 track_id=1041 az=045.3 el=012.9 range=8150m updates=2
2025-07-15T14:23:01.150Z [WARN ] [NETWORK   ] EVT_SEQ_GAP         src=0x0012 expected=10044 got=10046 gap=2
2025-07-15T14:23:01.200Z [INFO ] [ENGAGEMENT] EVT_WEAPON_STATUS   weapon_id=3 mode=ENGAGING rounds=460 barrel_temp=87C burst=4
2025-07-15T14:23:01.500Z [INFO ] [TRACKING  ] EVT_TRACK_NEW       src=0x0014 track_id=1043 class=ROCKET_ARTILLERY az=120.5 el=032.1 range=6100m
2025-07-15T14:23:01.501Z [ALARM] [THREAT    ] EVT_THREAT_CRITICAL track_id=1043 threat=CRITICAL class=ROCKET_ARTILLERY vel=-450m/s range=6080m closing
2025-07-15T14:23:02.001Z [WARN ] [TRACKING  ] EVT_TRACK_LOST      track_id=1042 last_update=900ms_ago reason=ENGAGED
2025-07-15T14:23:02.002Z [INFO ] [ENGAGEMENT] EVT_ENGAGE_CEASE    weapon_id=3 track_id=1042 reason=TARGET_DESTROYED rounds_used=20
2025-07-15T14:23:05.000Z [DEBUG] [HEALTH    ] EVT_HEARTBEAT_OK    subsystem=RADAR_PRIMARY state=OK cpu=34% mem=41% uptime=14523s
2025-07-15T14:23:05.001Z [WARN ] [HEALTH    ] EVT_HEARTBEAT_DEGRADE subsystem=EO_TRACKER state=DEGRADED cpu=92% mem=78% uptime=14520s err=0x0012
2025-07-15T14:23:05.010Z [ERROR] [NETWORK   ] EVT_CRC_FAIL        src=0x0018 seq=20411 expected_crc=0xA3F1B20C got_crc=0x00000000
2025-07-15T14:23:10.000Z [INFO ] [CONTROL   ] EVT_CONFIG_CHANGE   key=LOG_LEVEL value=DEBUG client=10.0.0.5:4412
```

---

## 4. System Architecture

### 4.1 Layer Diagram

```
+------------------------------------------------------------------+
|                        Operator CLI (TCP)                         |
+------------------------------------------------------------------+
        |                                           ^
        | TCP commands                              | TCP responses
        v                                           |
+------------------------------------------------------------------+
|                     Control Node (TCP Server)                     |
|  - Length-prefix framing                                          |
|  - Command dispatch (GET/SET/SUBSCRIBE)                           |
|  - Pushes stats and subscribed telemetry to connected clients     |
+------------------------------------------------------------------+
        |                       ^
        | config/subscribe      | stats queries
        v                       |
+------------------------------------------------------------------+
|                       Gateway Core                                |
|  +--------------------+  +------------------+  +---------------+  |
|  | TelemetryParser    |  | SequenceTracker  |  | StatsManager  |  |
|  | - validate header  |  | - per-src state  |  | - counters    |  |
|  | - check CRC        |  | - gap detection  |  | - health      |  |
|  | - decode payload   |  | - reorder detect |  | - per-source  |  |
|  +--------------------+  +------------------+  +---------------+  |
|  +--------------------+  +------------------+  +---------------+  |
|  | EventBus           |  | FrameRecorder    |  | Logger        |  |
|  | - publish/subscribe |  | - write to disk  |  | - structured  |  |
|  | - event routing    |  | - index by time  |  | - severity    |  |
|  +--------------------+  +------------------+  +---------------+  |
+------------------------------------------------------------------+
        ^                                           ^
        | UDP datagrams                             | Replay frames
        |                                           |
+---------------------+                 +----------------------+
| Sensor Simulator(s) |                 | Replay Tool          |
| - world model       |                 | - read recorded file |
| - object generator  |                 | - pace control       |
| - fault injection   |                 | - inject into gateway|
+---------------------+                 +----------------------+
```

### 4.2 Module Decomposition

```
NetNodeGateway/
|
+-- README.md
+-- REQUIREMENTS.md
+-- ARCHITECTURE.md              <-- this file
+-- CMakeLists.txt               <-- top-level build
|
+-- src/
|   +-- common/
|   |   +-- protocol.h           // TelemetryFrame, TrackPayload, PlotPayload, etc.
|   |   +-- events.h             // Event enum, EventRecord struct
|   |   +-- crc32.h / crc32.cpp  // CRC32 implementation (or use zlib)
|   |   +-- logger.h / logger.cpp// Structured logger with severity & category
|   |   +-- types.h              // Common typedefs, enums, constants
|   |
|   +-- gateway/
|   |   +-- gateway.h / gateway.cpp              // Main gateway loop
|   |   +-- telemetry_parser.h / .cpp            // Frame validation & decode
|   |   +-- sequence_tracker.h / .cpp            // Per-source sequence state
|   |   +-- stats_manager.h / .cpp               // Counters and health
|   |   +-- event_bus.h / .cpp                   // Pub/sub for internal events
|   |   +-- frame_recorder.h / .cpp              // Binary recording to disk
|   |
|   +-- sensor_sim/
|   |   +-- sensor_sim_main.cpp                  // Entry point
|   |   +-- world_model.h / .cpp                 // Maintains airborne objects
|   |   +-- object_generator.h / .cpp            // Random object spawning
|   |   +-- measurement_generator.h / .cpp       // PLOT/TRACK from world objects
|   |   +-- fault_injector.h / .cpp              // Loss, reorder, duplication
|   |   +-- scenario_profiles.h                  // idle, patrol, raid, stress configs
|   |
|   +-- control_node/
|   |   +-- control_node.h / .cpp                // TCP server main loop
|   |   +-- tcp_framer.h / .cpp                  // Length-prefix framing
|   |   +-- command_handler.h / .cpp             // Parse & dispatch commands
|   |
|   +-- cli/
|   |   +-- cli_main.cpp                         // Operator CLI entry point
|   |   +-- cli_client.h / .cpp                  // TCP client logic
|   |
|   +-- replay/
|       +-- replay_main.cpp                      // Replay tool entry point
|       +-- replay_engine.h / .cpp               // Pacing, frame injection
|
+-- tests/
|   +-- test_protocol_parser.cpp
|   +-- test_sequence_tracker.cpp
|   +-- test_tcp_framer.cpp
|   +-- test_crc32.cpp
|   +-- test_event_bus.cpp
|   +-- test_object_generator.cpp
|   +-- test_integration.cpp
|
+-- scenarios/
|   +-- idle.json                                // Scenario config files
|   +-- patrol.json
|   +-- raid.json
|   +-- stress.json
|
+-- recorded/                                    // Default output dir for recordings
```

### 4.3 Key Interfaces

#### Gateway receives frames via a `FrameSource` interface:

```cpp
class IFrameSource {
public:
    virtual ~IFrameSource() = default;
    // Returns true if a frame was received, fills out 'frame'
    virtual bool receive(TelemetryFrame& frame,
                         std::chrono::steady_clock::time_point& rx_time) = 0;
};

class UdpFrameSource : public IFrameSource { /* real UDP socket */ };
class ReplayFrameSource : public IFrameSource { /* reads from disk */ };
```

This abstraction allows the gateway to run identically from live UDP or replay,
satisfying the determinism requirement (NFR-2).

#### Events flow through an EventBus:

```cpp
using EventCallback = std::function<void(const EventRecord&)>;

class EventBus {
public:
    void subscribe(EventCategory cat, EventCallback cb);
    void publish(EventRecord event);
};
```

The Logger, StatsManager, ControlNode, and FrameRecorder all subscribe to
relevant event categories.

---

## 5. Implementation Phases

### Phase 1: Foundation (no sockets)
Build and test core logic without any network I/O.

| Step | Deliverable | Test |
|------|------------|------|
| 1.1 | `protocol.h` -- all struct definitions, enums, serialization | Compile test |
| 1.2 | `crc32.cpp` -- CRC32 compute/verify | Unit test with known vectors |
| 1.3 | `telemetry_parser.cpp` -- validate & decode frames from a byte buffer | Unit test: valid, truncated, bad CRC, bad version |
| 1.4 | `sequence_tracker.cpp` -- per-source gap/reorder/dup detection | Unit test: normal, gap, reorder, duplicate |
| 1.5 | `logger.cpp` -- structured log output (stdout + file) | Manual / unit test |
| 1.6 | `events.h` + `event_bus.cpp` -- publish/subscribe | Unit test |
| 1.7 | `stats_manager.cpp` -- counters, per-source stats, health | Unit test |

### Phase 2: Sensor Simulation (no sockets yet, generate to buffer)
Build the world model and object/measurement generation.

| Step | Deliverable | Test |
|------|------------|------|
| 2.1 | `object_generator.cpp` -- random airborne object spawning | Unit test: correct distributions |
| 2.2 | `world_model.cpp` -- advance positions, manage lifetimes | Unit test: objects move, expire |
| 2.3 | `measurement_generator.cpp` -- PLOT/TRACK from world model | Unit test: output frame validity |
| 2.4 | `fault_injector.cpp` -- drop/reorder/duplicate frames | Unit test: verify fault rates |
| 2.5 | `scenario_profiles.h` -- idle/patrol/raid/stress configs | Config loading test |
| 2.6 | End-to-end in-memory test: generator -> parser -> tracker -> stats | Integration test |

### Phase 3: Network Layer
Add real UDP and TCP sockets.

| Step | Deliverable | Test |
|------|------------|------|
| 3.1 | `UdpFrameSource` -- bind socket, receive datagrams | Loopback test |
| 3.2 | `sensor_sim_main.cpp` -- send generated frames over UDP | sensor_sim -> gateway loopback |
| 3.3 | `tcp_framer.cpp` -- length-prefix encode/decode | Unit test: partial read, multi-frame |
| 3.4 | `control_node.cpp` -- accept connections, dispatch commands | Loopback test |
| 3.5 | `command_handler.cpp` -- GET HEALTH/STATS, SET, SUBSCRIBE | Unit test |
| 3.6 | `cli_client.cpp` -- send commands, display responses | Manual test |

### Phase 4: Record / Replay
Add persistence and deterministic replay.

| Step | Deliverable | Test |
|------|------------|------|
| 4.1 | `frame_recorder.cpp` -- write frames to binary file with timestamps | Unit test |
| 4.2 | `ReplayFrameSource` -- read recorded file, pace output | Unit test |
| 4.3 | `replay_main.cpp` -- CLI for replay tool | record -> replay -> verify identical stats |

### Phase 5: Integration & Hardening
Full system integration, stress testing, and polish.

| Step | Deliverable | Test |
|------|------------|------|
| 5.1 | Full integration test: sensor_sim -> gateway -> control_node | Automated |
| 5.2 | Stress test with `stress` scenario profile | Performance counters |
| 5.3 | Fault injection at network level (iptables or built-in) | Verify counters |
| 5.4 | Record + replay determinism verification | Bit-identical stats |
| 5.5 | Log review: verify structured output matches example format | Manual |

---

## 6. Build System

CMake structure:

```cmake
cmake_minimum_required(VERSION 3.16)
project(NetNodeGateway LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# Library: common (protocol, crc, logger, events)
add_library(nng_common STATIC
    src/common/crc32.cpp
    src/common/logger.cpp
    src/common/event_bus.cpp
)

# Library: gateway core (no sockets)
add_library(nng_gateway_core STATIC
    src/gateway/telemetry_parser.cpp
    src/gateway/sequence_tracker.cpp
    src/gateway/stats_manager.cpp
    src/gateway/frame_recorder.cpp
)
target_link_libraries(nng_gateway_core PUBLIC nng_common)

# Library: sensor simulation (no sockets)
add_library(nng_sensor_sim STATIC
    src/sensor_sim/object_generator.cpp
    src/sensor_sim/world_model.cpp
    src/sensor_sim/measurement_generator.cpp
    src/sensor_sim/fault_injector.cpp
)
target_link_libraries(nng_sensor_sim PUBLIC nng_common)

# Executables
add_executable(gateway src/gateway/gateway.cpp)
target_link_libraries(gateway PRIVATE nng_gateway_core)

add_executable(sensor_sim src/sensor_sim/sensor_sim_main.cpp)
target_link_libraries(sensor_sim PRIVATE nng_sensor_sim)

add_executable(control_node
    src/control_node/control_node.cpp
    src/control_node/tcp_framer.cpp
    src/control_node/command_handler.cpp
)
target_link_libraries(control_node PRIVATE nng_gateway_core)

add_executable(cli src/cli/cli_main.cpp src/cli/cli_client.cpp)
target_link_libraries(cli PRIVATE nng_common)

add_executable(replay
    src/replay/replay_main.cpp
    src/replay/replay_engine.cpp
)
target_link_libraries(replay PRIVATE nng_gateway_core)

# Tests (using GoogleTest or Catch2)
enable_testing()
# ... test targets ...
```

Key design decisions:
- **Core libraries have no socket dependencies** -- testable without network
- **Static libraries** keep linking simple for a single-machine simulation
- **C++17** for `std::optional`, `std::variant`, structured bindings, `<filesystem>`
- **No external dependencies** beyond a test framework (GoogleTest or Catch2)

---

## 7. Threading Model

```
Thread 1: Gateway main loop
  - poll UDP socket (or replay source)
  - parse frame
  - update sequence tracker
  - publish events
  - write to recorder (if enabled)

Thread 2: Control node TCP server
  - accept connections
  - read/write framed TCP messages
  - query stats_manager (lock-protected)

Thread 3 (optional): Sensor simulator
  - advance world model
  - generate and send frames
  - runs in same or separate process
```

Synchronization:
- `StatsManager` uses `std::shared_mutex` (readers = TCP queries, writer = gateway loop)
- `EventBus` uses a lock-free queue or mutex-protected deque
- No shared state between sensor simulator and gateway (connected only via UDP)

---

## 8. Configuration

Runtime configuration via a simple key-value scheme (loadable from file or
TCP `SET` commands):

```
# gateway.conf
udp.listen_port = 5000
udp.recv_buffer_size = 65536
tcp.listen_port = 6000
tcp.max_clients = 8
log.level = INFO
log.file = gateway.log
log.structured = true
crc.enabled = true
record.enabled = false
record.path = ./recorded/session.bin
sim.profile = patrol
sim.target_rate_hz = 50
sim.fault.loss_pct = 2.0
sim.fault.reorder_pct = 0.5
sim.fault.duplicate_pct = 0.1
```

---

## 9. How to Proceed

**Recommended next action:** Start Phase 1 implementation.

1. Create the directory structure (`src/common/`, `src/gateway/`, `tests/`, etc.)
2. Implement `protocol.h` with all struct definitions and serialization
3. Implement `crc32.cpp` with unit tests
4. Implement `telemetry_parser.cpp` with unit tests
5. Implement `sequence_tracker.cpp` with unit tests
6. Set up CMake build with GoogleTest

Phase 1 produces a fully testable core with zero socket dependencies. Every
subsequent phase adds a layer on top of tested foundations.

The key architectural invariant: **no module above the network boundary depends
on socket APIs directly.** All frame I/O goes through `IFrameSource` /
`IFrameSink` interfaces, making the system testable, replayable, and
deterministic by construction.
