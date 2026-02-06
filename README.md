# NetNodeGateway

> **This repository is created for the Rheinmetall interview on 6.2.26 with Thomas Fueeli and to manifest comprehension of TCP/UDP protocol usage in a closed-loop system and relevant C++ coding abilities.**

**C++ UDP/TCP Closed-Loop Sensor/Control Network Simulation (Gateway + Record/Replay + Debuggable Protocols)**

## Why this project exists
Industrial and defense-grade distributed systems commonly use:
- **UDP** for fast, high-rate telemetry (latest data matters most; loss is tolerated)
- **TCP** for reliable control/configuration/diagnostics (commands must arrive exactly once)

This repository demonstrates that engineering reality in a **closed-loop, offline** setup:
- a sensor simulator emits telemetry over UDP
- a gateway ingests and validates frames, tracks loss/reorder/gaps, and optionally records to disk
- a TCP control node provides health/stats and runtime configuration
- a replay tool allows deterministic reproduction of bugs from recorded traffic

## System Overview
Components:
- `sensor_sim_xtar3d` (UDP Producer)
  - emits telemetry frames at configurable rate
  - can inject loss/reorder/duplication to mimic real network conditions
- `gateway` (UDP Ingest + Parser + Stats + Recorder)
  - receives UDP datagrams, validates protocol, updates per-source stats
  - records frames to disk for later replay
- `control_node` (TCP Control Plane)
  - framed TCP commands for health/stats/config/subscriptions
- `cli` (TCP Client)
  - sends commands to control node (human-friendly)
- `replay` (Record/Replay Utility)
  - replays recorded telemetry at real-time or accelerated speed

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              NetNodeGateway                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────┐         UDP (port 5000)         ┌─────────────────┐   │
│  │  sensor_sim     │ ───────────────────────────────▶│    gateway      │   │
│  │                 │    Telemetry Frames             │                 │   │
│  │  - ObjectGen    │    (PLOT/TRACK/HEARTBEAT)       │  - UdpSocket    │   │
│  │  - WorldModel   │                                 │  - Parser       │   │
│  │  - MeasureGen   │                                 │  - SeqTracker   │   │
│  │  - FaultInject  │                                 │  - StatsManager │   │
│  └─────────────────┘                                 │  - Recorder     │   │
│                                                      └────────┬────────┘   │
│                                                               │            │
│                                                               │ record     │
│                                                               ▼            │
│  ┌─────────────────┐         TCP (port 5001)         ┌─────────────────┐   │
│  │    cli          │ ◀──────────────────────────────▶│  control_node   │   │
│  │                 │    Commands/Responses           │                 │   │
│  │  - TcpClient    │    (GET STATS, SET CONFIG)      │  - TcpFramer    │   │
│  └─────────────────┘                                 │  - CmdHandler   │   │
│                                                      └─────────────────┘   │
│                                                                             │
│  ┌─────────────────┐                                 ┌─────────────────┐   │
│  │    replay       │ ◀─────────────────────────────  │  recorded/*.bin │   │
│  │                 │    Replay recorded frames       │                 │   │
│  │  - ReplayEngine │                                 │  (Frame files)  │   │
│  └─────────────────┘                                 └─────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Module Structure

```
src/
├── common/              # Shared utilities
│   ├── crc32.cpp/h      # CRC32 checksum
│   ├── logger.cpp/h     # Timestamped logging
│   ├── event_bus.cpp/h  # Publish/subscribe events
│   ├── protocol.h       # Frame serialization
│   └── types.h          # Shared type definitions
│
├── gateway/             # UDP telemetry ingestion
│   ├── gateway.cpp/h    # Main orchestrator
│   ├── udp_socket.cpp/h # UDP receive/send
│   ├── telemetry_parser.cpp/h  # Frame parsing & validation
│   ├── sequence_tracker.cpp/h  # Gap/reorder detection
│   ├── stats_manager.cpp/h     # Per-source statistics
│   ├── frame_recorder.cpp/h    # Write frames to disk
│   └── frame_source.h          # Abstract frame source
│
├── sensor_sim/          # Telemetry producer
│   ├── object_generator.cpp/h     # Spawn simulated objects
│   ├── world_model.cpp/h          # Track object state
│   ├── measurement_generator.cpp/h # Generate telemetry
│   ├── fault_injector.cpp/h       # Inject loss/reorder/dup
│   └── scenario_loader.cpp/h      # Load scenario profiles
│
├── control_node/        # TCP control plane
│   ├── control_node.cpp/h    # TCP server
│   ├── tcp_framer.cpp/h      # Length-prefix framing
│   └── command_handler.cpp/h # Command processing
│
├── cli/                 # Command-line client
│   └── cli_client.cpp/h # TCP client for control
│
└── replay/              # Deterministic replay
    └── replay_engine.cpp/h # Replay recorded frames
```

### Data Flow

1. **Sensor Simulation**: `ObjectGenerator` spawns objects → `WorldModel` updates positions → `MeasurementGenerator` creates telemetry frames
2. **Fault Injection**: `FaultInjector` applies configurable loss/reorder/duplicate/corrupt
3. **UDP Transport**: Frames sent via UDP socket to gateway
4. **Gateway Processing**: `TelemetryParser` validates → `SequenceTracker` detects anomalies → `StatsManager` records metrics
5. **Recording**: `FrameRecorder` writes timestamped frames to disk
6. **Replay**: `ReplayEngine` reads recorded frames and re-injects at original timing

### Why UDP?
UDP is used for telemetry streams because:
- it has low overhead and low latency
- packet loss is acceptable for many telemetry streams (next update arrives soon)
- ordering can be handled at the application layer (sequence numbers)

### Why TCP?
TCP is used for control because:
- command delivery must be reliable
- message framing can be enforced
- reconnect logic and acknowledgements are straightforward

## Protocol (Telemetry Frame)
Each UDP datagram contains one `TelemetryFrame`:

Header fields:
- `version` (u8)
- `msg_type` (u8)  // PLOT=1, TRACK=2, HEARTBEAT=3
- `src_id` (u16)
- `seq` (u32)
- `ts_ns` (u64)
- `payload_len` (u16)
- `payload` (bytes[payload_len])
- optional `crc32` (u32)

The gateway validates:
- supported `version`
- length bounds
- CRC (if enabled)
- sequence continuity per `src_id`

## TCP Control Protocol
TCP is framed using **length-prefix framing**:
- `[u32_be length][payload bytes...]`

Example commands (ASCII payloads):
- `GET HEALTH`
- `GET STATS`
- `SET LOG_LEVEL=DEBUG`
- `SET CRC=ON`
- `SUBSCRIBE SENSOR=all`

## Build & Run

### Quick Start (Makefile)
```bash
make          # Build and run demo (default)
make build    # Build only
make test     # Run all unit tests
make help     # Show all available targets
```

### Manual Build (CMake)
```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -j$(nproc)


