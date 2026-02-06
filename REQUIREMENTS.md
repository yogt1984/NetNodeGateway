# REQUIREMENTS.md

## Project: NetNodeGateway (Closed-Loop Sensor/Control Network Simulation)

### 1. Purpose
NetNodeGateway is a C++ project that simulates a **closed-loop, offline** networked system where:
- one or more **sensors** emit high-rate telemetry over **UDP**
- a **gateway node** ingests, validates, and processes telemetry
- a **control node** provides reliable **TCP** control, diagnostics, and configuration
- the system supports **record/replay** to reproduce bugs deterministically

The project is designed to demonstrate:
- practical UDP/TCP usage patterns
- robust parsing and fault tolerance
- debugging and testability for networked systems
- clean architecture suitable for integration into larger C++ codebases

### 2. Operating Assumptions
- The system runs in an **offline, closed-loop network** (no Internet dependency).
- UDP telemetry sources may be unreliable: packet loss, duplication, reordering are expected.
- TCP control clients may connect/disconnect at any time.
- The system must remain operational under malformed inputs.

### 3. System Context
Actors:
- **Sensor Simulator(s)** (UDP producers)
- **Gateway Service** (UDP ingest, parsing, stats, record/replay)
- **Control Node** (TCP server, runtime control, observability)
- **Operator CLI** (TCP client)

Data flows:
- UDP: sensor → gateway (telemetry stream)
- TCP: operator → control node (commands), control node → operator (responses/stream)
- File: gateway ↔ disk (record/replay)

### 4. Functional Requirements

#### FR-1 UDP Telemetry Ingestion
- The gateway shall listen on a configured UDP port.
- The gateway shall accept datagrams from multiple sources.
- The gateway shall parse telemetry frames without buffer overruns.
- The gateway shall detect and count:
  - malformed frames
  - sequence gaps
  - out-of-order frames
  - duplicate frames (optional)

#### FR-2 Message Format & Validation
Each telemetry frame shall include:
- protocol version
- message type (PLOT/TRACK/HEARTBEAT)
- source ID
- sequence number
- timestamp (ns)
- payload length
- payload bytes
Optional:
- CRC32 checksum

The gateway shall reject frames with:
- unsupported version
- invalid lengths
- CRC mismatch (if enabled)

#### FR-3 TCP Control Plane
A control node shall expose a TCP interface to:
- query health: `GET HEALTH`
- query statistics: `GET STATS`
- change runtime config: `SET <key>=<value>`
- subscribe to telemetry stream: `SUBSCRIBE SENSOR=<id|all>`

TCP shall implement framing (length-prefix) to avoid message-boundary bugs.

#### FR-4 Record & Replay
- The gateway shall support recording received telemetry to disk.
- The replay tool shall replay recorded frames:
  - real-time speed
  - accelerated speed
- Replay shall preserve original ordering and timestamps (within replay mode constraints).

#### FR-5 Observability
The system shall expose:
- counters: rx_total, malformed_total, gap_total, reorder_total, dup_total (opt)
- per-source stats (by src_id)
- component health state: OK / DEGRADED / ERROR
Logs shall be timestamped and structured.

### 5. Non-Functional Requirements

#### NFR-1 Reliability
- No single malformed UDP frame shall crash the process.
- TCP disconnects shall not affect UDP ingestion.

#### NFR-2 Determinism
- Given the same recorded input and configuration, replay results shall be reproducible.

#### NFR-3 Performance
- The gateway shall support sustained input rate configurable (e.g., 100–10,000 packets/s).
- The gateway shall remain responsive to control commands under load.

#### NFR-4 Maintainability
- Protocol parsing shall be isolated from socket code.
- Network transport shall be isolated from business logic.
- Components shall be testable with unit tests that run without sockets where possible.

### 6. Verification Strategy
- Unit tests:
  - TCP framer (partial reads, multiple frames per read)
  - protocol parser (valid/invalid, truncation, CRC mismatch)
  - sequence tracker (gaps, reorder, duplicates)
- Integration tests:
  - sensor simulator → gateway → stats correctness
  - packet loss/reorder injection → counters increment
  - record + replay → identical counts and outputs

### 7. Out of Scope
- GUI
- Cloud deployment
- External authentication / PKI
- Real radar physics or proprietary domain algorithms

