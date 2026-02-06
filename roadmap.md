# NetNodeGateway — Simulation Implementation Roadmap

This roadmap breaks down the implementation of the **closed-loop sensor/control network simulation** described in [REQUIREMENTS.md](REQUIREMENTS.md) and [README.md](README.md) into ordered phases and concrete tasks.

---

## Overview

**Goal:** Implement a working simulation where sensor simulators send UDP telemetry to a gateway, a control node serves TCP commands, and record/replay enables deterministic debugging.

**Deliverables (executables):**
- `sensor_sim_xtar3d` — UDP telemetry producer (with optional fault injection)
- `gateway` — UDP ingest, parsing, stats, recording
- `control_node` — TCP server (health, stats, config, subscriptions)
- `cli` — TCP client for operator commands
- `replay` — Replay recorded telemetry (real-time or accelerated)

---

## Phase 0: Project Foundation

| ID   | Task | Notes |
|------|------|--------|
| 0.1  | **CMake project layout** — Root `CMakeLists.txt`, `src/`, `include/`, `tests/`, optional `tools/` or `apps/`. | Match README “CMake-first” build. |
| 0.2  | **Build profiles** — Debug/Release, compiler warnings, C++ standard (e.g. C++17). | Portable, clean builds. |
| 0.3  | **Telemetry frame types** — Define `TelemetryFrame` struct and constants (version, PLOT/TRACK/HEARTBEAT, payload limits). | Header-only or small lib; no I/O yet. |
| 0.4  | **TCP frame types** — Length-prefix type (e.g. `uint32_t` big-endian length + payload). | Shared between control node and CLI. |

**Exit criteria:** Project builds; types compile and can be unit-tested (e.g. size/alignment checks).

---

## Phase 1: Protocol Parsing & Validation (No Sockets)

| ID   | Task | Notes |
|------|------|--------|
| 1.1  | **Telemetry frame parser** — Parse raw bytes → `TelemetryFrame`; validate version, length bounds, optional CRC32. | Isolate parsing from transport (REQ §4 FR-2, §5 NFR-4). |
| 1.2  | **Parser fault handling** — Reject unsupported version, invalid lengths, CRC mismatch; never overflow buffers. | FR-2; NFR-1 (no crash on malformed input). |
| 1.3  | **Unit tests: parser** — Valid frames, truncated input, wrong version, bad length, CRC mismatch. | REQ §6 verification. |

**Exit criteria:** Parser library passes unit tests without using any sockets.

---

## Phase 2: Sequence Tracking & Statistics

| ID   | Task | Notes |
|------|------|--------|
| 2.1  | **Per-source sequence tracker** — For each `src_id`, track last `seq`; detect gap, reorder, optional duplicate. | FR-1 (sequence gaps, out-of-order, duplicates). |
| 2.2  | **Stats counters** — `rx_total`, `malformed_total`, `gap_total`, `reorder_total`, `dup_total` (optional); per-source stats. | FR-5 observability. |
| 2.3  | **Unit tests: sequence tracker** — In-order, single gap, reorder, duplicate; multiple `src_id`s. | REQ §6. |

**Exit criteria:** Sequence logic and counters are testable in isolation; no network code.

---

## Phase 3: UDP Gateway Service

| ID   | Task | Notes |
|------|------|--------|
| 3.1  | **UDP listen** — Bind to configured port; non-blocking or dedicated thread/loop. | FR-1: gateway listens on UDP port. |
| 3.2  | **Ingest loop** — Read datagrams, pass to parser, feed valid frames to sequence tracker and stats; count malformed. | FR-1, FR-2; NFR-1. |
| 3.3  | **Config** — UDP port, optional CRC on/off, payload limits (e.g. max payload size). | Align with FR-2 and NFR-3 (sustained rate). |
| 3.4  | **Integration test** — External or in-process UDP sender → gateway; assert stats (e.g. rx_total, one malformed). | REQ §6 integration tests. |

**Exit criteria:** Gateway receives UDP, updates stats and sequence state correctly under valid and invalid frames.

---

## Phase 4: TCP Control Plane

| ID   | Task | Notes |
|------|------|--------|
| 4.1  | **TCP framer** — Encode/decode length-prefix frames (e.g. u32_be length + payload). Handle partial reads and multiple frames per read. | FR-3; REQ §6 unit tests (framer). |
| 4.2  | **Control node TCP server** — Accept connections; serve framed requests on a configurable port. | FR-3. |
| 4.3  | **Command handlers** — `GET HEALTH`, `GET STATS`, `SET <key>=<value>`, `SUBSCRIBE SENSOR=<id\|all>`. Health = OK/DEGRADED/ERROR; stats from gateway. | FR-3, FR-5. |
| 4.4  | **Control–gateway link** — Control node obtains stats/health from gateway (shared memory, queue, or in-process API). | So GET STATS / GET HEALTH reflect real gateway state. |
| 4.5  | **Unit tests: TCP framer** — Partial reads, multiple frames in one read, oversized length handling. | REQ §6. |

**Exit criteria:** Control node runs; CLI (or raw TCP client) can send commands and receive framed responses; TCP disconnect does not affect UDP (NFR-1).

---

## Phase 5: Sensor Simulator

| ID   | Task | Notes |
|------|------|--------|
| 5.1  | **UDP sender** — Build valid `TelemetryFrame` (version, msg_type, src_id, seq, ts_ns, payload); send to gateway UDP address/port. | README: sensor_sim_xtar3d emits telemetry. |
| 5.2  | **Rate control** — Configurable packets/sec (e.g. 100–10,000); steady or burst. | NFR-3; README “configurable rate”. |
| 5.3  | **Fault injection (optional)** — Drop, reorder, or duplicate packets to mimic bad network. | README “inject loss/reorder/duplication”; integration test REQ §6. |
| 5.4  | **Binary: sensor_sim_xtar3d** — CLI args for gateway host/port, rate, src_id, fault options. | Runnable simulator. |

**Exit criteria:** `sensor_sim_xtar3d` sends UDP telemetry at configured rate; gateway stats match (with or without fault injection).

---

## Phase 6: Record & Replay

| ID   | Task | Notes |
|------|------|--------|
| 6.1  | **Record format** — Define file format (e.g. timestamp + raw frame bytes, or timestamp + serialized frame). | FR-4: gateway records to disk. |
| 6.2  | **Gateway recording** — On option, write each received valid frame (and optionally timestamp) to file. | FR-4. |
| 6.3  | **Replay tool** — Read file; send frames to gateway (or dedicated replay port) at real-time or accelerated speed. | FR-4; README “replay” at real-time or accelerated. |
| 6.4  | **Determinism** — Same recording + config → same replay outcome (counts, order). | NFR-2. |
| 6.5  | **Integration test** — Record session → replay → compare stats/counts with original run. | REQ §6. |

**Exit criteria:** Record a run, replay it; stats and ordering are reproducible.

---

## Phase 7: Operator CLI

| ID   | Task | Notes |
|------|------|--------|
| 7.1  | **TCP client** — Connect to control node; send framed commands (GET HEALTH, GET STATS, SET …, SUBSCRIBE …). | README: cli sends commands to control node. |
| 7.2  | **Human-friendly interface** — Parse simple commands from stdin or args; print responses. | README “human-friendly”. |
| 7.3  | **Binary: cli** — Executable; configurable control node host/port. | Operator can run cli against running control_node. |

**Exit criteria:** Operator uses `cli` to query health/stats, change config, subscribe to telemetry stream.

---

## Phase 8: Observability & Hardening

| ID   | Task | Notes |
|------|------|--------|
| 8.1  | **Structured logging** — Timestamped, structured logs (e.g. component, level, message). | FR-5. |
| 8.2  | **Health state** — Gateway/control node expose OK / DEGRADED / ERROR from configurable logic (e.g. error rate thresholds). | FR-5. |
| 8.3  | **Subscription stream** — SUBSCRIBE SENSOR=id|all streams telemetry (or summaries) over TCP to client. | FR-3. |
| 8.4  | **Load test** — Sustained 100–10k packets/s; control commands remain responsive. | NFR-3. |
| 8.5  | **Integration test matrix** — Sensor sim → gateway → stats; packet loss/reorder injection → counters; record → replay → same counts. | REQ §6. |

**Exit criteria:** Logs and health are consistent; subscriptions work; performance and integration tests pass.

---

## Dependency Summary

```
Phase 0 (Foundation)
    ↓
Phase 1 (Parser) ──────────────────┐
    ↓                              ↓
Phase 2 (Sequence/Stats)     Phase 4 (TCP Framer)
    ↓                              ↓
Phase 3 (UDP Gateway)        Phase 4 (Control Node)
    ↓                              ↓
Phase 5 (Sensor Sim)         Phase 7 (CLI)
    ↓
Phase 6 (Record/Replay)
    ↓
Phase 8 (Observability & Hardening)
```

- **Phase 4** can start once the TCP framer and types (0.4) are done; control node needs gateway stats (Phase 3) for GET STATS/HEALTH.
- **Phase 5** depends on Phase 3 (gateway listening).
- **Phase 6** depends on Phase 3 (recording) and a replay consumer (gateway or separate ingest).
- **Phase 7** depends on Phase 4 (control node and protocol).

---

## Suggested Implementation Order (Single Track)

1. **0** → **1** → **2** (foundation, parsing, stats; all testable without I/O).
2. **3** (UDP gateway) so the rest of the loop has a sink.
3. **4** (TCP control plane) in parallel or after 3.
4. **5** (sensor sim) to drive the gateway.
5. **6** (record/replay) for determinism and debugging.
6. **7** (CLI) for operator interaction.
7. **8** (observability and integration tests) to satisfy NFRs and verification strategy.

---

## Out of Scope (per REQUIREMENTS §7)

- GUI
- Cloud deployment
- External authentication / PKI
- Real radar/sensor physics or proprietary algorithms

This roadmap stays within the closed-loop, offline simulation and record/replay scope described in the requirements and README.
