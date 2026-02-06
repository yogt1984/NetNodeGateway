# TASKS.md

Atomic implementation tasks for the NetNodeGateway project. Each task is
designed to be implemented and tested reliably by the Sonnet model in a single
session. Tasks are small, self-contained, have explicit inputs/outputs, and
include concrete acceptance criteria.

**Conventions:**
- Tasks are numbered `P<phase>.<sequence>` (e.g., `P1.01`)
- `depends:` lists tasks that must be complete before starting
- `files:` lists exactly which files to create or modify
- `test:` describes how to verify the task is done correctly
- `acceptance:` lists concrete pass/fail criteria
- All code is C++17, compiled with CMake, tested with GoogleTest

---

## Phase 0: Project Scaffolding

### P0.01 -- Create directory structure

**depends:** none
**files:** directories only

Create the following empty directory tree:

```
src/common/
src/gateway/
src/sensor_sim/
src/control_node/
src/cli/
src/replay/
tests/
scenarios/
recorded/
```

**test:** `ls -R` shows all directories exist
**acceptance:**
- All 8 directories exist
- No source files yet (just dirs)

---

### P0.02 -- Create top-level CMakeLists.txt with GoogleTest

**depends:** P0.01
**files:** `CMakeLists.txt`

Create the root `CMakeLists.txt` that:
- Sets `cmake_minimum_required(VERSION 3.16)`
- Sets project name `NetNodeGateway`, language `CXX`
- Sets `CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_STANDARD_REQUIRED ON`
- Fetches GoogleTest via `FetchContent` (tag `v1.14.0`)
- Enables `testing`
- Includes `src/common/CMakeLists.txt` via `add_subdirectory`
- Does NOT yet define any libraries or targets (those come in later tasks)

Also create a minimal `src/common/CMakeLists.txt` placeholder that defines an
INTERFACE library `nng_common` with no sources yet:

```cmake
add_library(nng_common INTERFACE)
target_include_directories(nng_common INTERFACE ${CMAKE_SOURCE_DIR}/src)
```

**test:** `mkdir build && cd build && cmake .. && cmake --build .` succeeds
with no errors
**acceptance:**
- CMake configure succeeds
- Build succeeds (nothing to compile yet, but no errors)
- GoogleTest is fetched

---

## Phase 1: Foundation (Pure Logic, No Sockets)

### P1.01 -- Define types.h (common enums and constants)

**depends:** P0.02
**files:** `src/common/types.h`

Create a header-only file with:

```cpp
#pragma once
#include <cstdint>

namespace nng {

// Protocol version
constexpr uint8_t PROTOCOL_VERSION = 1;

// Message types
enum class MsgType : uint8_t {
    PLOT            = 0x01,
    TRACK           = 0x02,
    HEARTBEAT       = 0x03,
    ENGAGEMENT      = 0x04,
};

// Track classification
enum class TrackClass : uint8_t {
    UNKNOWN         = 0x00,
    FIXED_WING      = 0x01,
    ROTARY_WING     = 0x02,
    UAV_SMALL       = 0x03,
    UAV_LARGE       = 0x04,
    MISSILE         = 0x05,
    ROCKET_ARTILLERY= 0x06,
    BIRD            = 0x07,
    DECOY           = 0x08,
};

// Threat level
enum class ThreatLevel : uint8_t {
    UNKNOWN  = 0, LOW = 1, MEDIUM = 2, HIGH = 3, CRITICAL = 4,
};

// IFF status
enum class IffStatus : uint8_t {
    NO_RESPONSE = 0, FRIEND = 1, FOE = 2, PENDING = 3,
};

// Subsystem state (heartbeat)
enum class SubsystemState : uint8_t {
    OK = 0, DEGRADED = 1, ERROR = 2, OFFLINE = 3,
};

// Weapon mode
enum class WeaponMode : uint8_t {
    SAFE = 0, ARMED = 1, ENGAGING = 2, CEASE_FIRE = 3,
};

// Log severity
enum class Severity : uint8_t {
    DEBUG = 0, INFO = 1, WARN = 2, ALARM = 3, ERROR = 4, FATAL = 5,
};

// Event category
enum class EventCategory : uint8_t {
    TRACKING = 0, THREAT = 1, IFF = 2, ENGAGEMENT = 3,
    NETWORK = 4, HEALTH = 5, CONTROL = 6,
};

// Event ID
enum class EventId : uint16_t {
    EVT_TRACK_NEW           = 0x0100,
    EVT_TRACK_UPDATE        = 0x0101,
    EVT_TRACK_LOST          = 0x0102,
    EVT_TRACK_CLASSIFY      = 0x0103,
    EVT_THREAT_EVAL         = 0x0200,
    EVT_THREAT_CRITICAL     = 0x0201,
    EVT_IFF_RESPONSE        = 0x0300,
    EVT_IFF_FOE             = 0x0301,
    EVT_ENGAGE_START        = 0x0400,
    EVT_ENGAGE_CEASE        = 0x0401,
    EVT_WEAPON_STATUS       = 0x0402,
    EVT_AMMO_LOW            = 0x0403,
    EVT_SEQ_GAP             = 0x0500,
    EVT_SEQ_REORDER         = 0x0501,
    EVT_FRAME_MALFORMED     = 0x0502,
    EVT_CRC_FAIL            = 0x0503,
    EVT_SOURCE_ONLINE       = 0x0504,
    EVT_SOURCE_TIMEOUT      = 0x0505,
    EVT_HEARTBEAT_OK        = 0x0600,
    EVT_HEARTBEAT_DEGRADE   = 0x0601,
    EVT_HEARTBEAT_ERROR     = 0x0602,
    EVT_CONFIG_CHANGE       = 0x0700,
};

// Telemetry frame header size (without payload)
// version(1) + msg_type(1) + src_id(2) + seq(4) + ts_ns(8) + payload_len(2) = 18
constexpr size_t FRAME_HEADER_SIZE = 18;
constexpr size_t FRAME_CRC_SIZE = 4;
constexpr size_t MAX_PAYLOAD_SIZE = 1024;

} // namespace nng
```

**test:** Include in a .cpp, static_assert sizes, compile
**acceptance:**
- Header compiles without warnings under `-Wall -Wextra`
- All enums have explicit underlying types
- Constants match the protocol spec in ARCHITECTURE.md

---

### P1.02 -- Define protocol.h (frame and payload structs)

**depends:** P1.01
**files:** `src/common/protocol.h`

Create a header with packed structs for wire format and serialize/deserialize
functions. All multi-byte fields are **little-endian** on the wire.

Define these structs (use `#pragma pack(push, 1)` for wire layout):

```cpp
struct TelemetryHeader {
    uint8_t  version;
    uint8_t  msg_type;
    uint16_t src_id;
    uint32_t seq;
    uint64_t ts_ns;
    uint16_t payload_len;
};
// static_assert(sizeof(TelemetryHeader) == 18)

struct PlotPayload {
    uint32_t plot_id;
    int32_t  azimuth_mdeg;
    int32_t  elevation_mdeg;
    uint32_t range_m;
    int16_t  amplitude_db;
    int16_t  doppler_mps;
    uint8_t  quality;
};
// static_assert(sizeof(PlotPayload) == 21)

struct TrackPayload {
    uint32_t track_id;
    uint8_t  classification;
    uint8_t  threat_level;
    uint8_t  iff_status;
    int32_t  azimuth_mdeg;
    int32_t  elevation_mdeg;
    uint32_t range_m;
    int16_t  velocity_mps;
    int16_t  rcs_dbsm;
    uint16_t update_count;
};
// static_assert(sizeof(TrackPayload) == 24)

struct HeartbeatPayload {
    uint16_t subsystem_id;
    uint8_t  state;
    uint8_t  cpu_pct;
    uint8_t  mem_pct;
    uint32_t uptime_s;
    uint16_t error_code;
};
// static_assert(sizeof(HeartbeatPayload) == 11)

struct EngagementPayload {
    uint16_t weapon_id;
    uint8_t  mode;
    uint32_t assigned_track;
    uint16_t rounds_remaining;
    int16_t  barrel_temp_c;
    uint16_t burst_count;
};
// static_assert(sizeof(EngagementPayload) == 13)
```

Also provide:
- `void serialize_header(const TelemetryHeader& h, uint8_t* buf)`
- `TelemetryHeader deserialize_header(const uint8_t* buf)`
- Similar for each payload type

These are simple `memcpy`-based since we use packed structs and target
little-endian x86.

**test:** Unit test: round-trip serialize/deserialize each struct, verify all
fields survive
**acceptance:**
- `static_assert` on every struct size passes
- Round-trip test passes for all 5 struct types
- Compiles with `-Wall -Wextra -Wpedantic` clean

---

### P1.03 -- Implement CRC32 (crc32.h / crc32.cpp)

**depends:** P0.02
**files:** `src/common/crc32.h`, `src/common/crc32.cpp`, `tests/test_crc32.cpp`

Implement CRC32 (ISO 3309 / ITU-T V.42, same polynomial as zlib: `0xEDB88320`).

Public API:
```cpp
namespace nng {
    uint32_t crc32(const uint8_t* data, size_t len);
    uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len);
}
```

Use a lookup table for speed (256-entry table, generated at compile time or
as a `constexpr` array).

Update `src/common/CMakeLists.txt` to make `nng_common` a STATIC library
(instead of INTERFACE) and add `crc32.cpp` as a source.

**test:** `tests/test_crc32.cpp` with GoogleTest:
- `crc32("", 0)` == `0x00000000`
- `crc32("123456789", 9)` == `0xCBF43926` (standard check value)
- `crc32` of a known 100-byte buffer matches a precomputed value
- Incremental `crc32_update` produces same result as single-call `crc32`

**acceptance:**
- All 4 test cases pass
- No external dependencies (no zlib)

---

### P1.04 -- Implement TelemetryParser (telemetry_parser.h / .cpp)

**depends:** P1.02, P1.03
**files:** `src/gateway/telemetry_parser.h`, `src/gateway/telemetry_parser.cpp`,
`src/gateway/CMakeLists.txt`, `tests/test_telemetry_parser.cpp`

The parser takes a raw byte buffer (a received UDP datagram) and produces a
validated, parsed result.

```cpp
namespace nng {

enum class ParseError {
    OK = 0,
    TOO_SHORT,          // buffer shorter than FRAME_HEADER_SIZE
    BAD_VERSION,        // version != PROTOCOL_VERSION
    BAD_MSG_TYPE,       // msg_type not in {1,2,3,4}
    PAYLOAD_TOO_LONG,   // payload_len > MAX_PAYLOAD_SIZE
    TRUNCATED,          // buffer shorter than header + payload_len (+ optional crc)
    CRC_MISMATCH,       // CRC present but doesn't match
};

struct ParsedFrame {
    TelemetryHeader header;
    const uint8_t*  payload_ptr;  // points into original buffer
    uint32_t        crc;          // 0 if CRC not present
    bool            has_crc;
};

// crc_enabled: if true, expect 4-byte CRC after payload
ParseError parse_frame(const uint8_t* buf, size_t len,
                       bool crc_enabled, ParsedFrame& out);

} // namespace nng
```

Create `src/gateway/CMakeLists.txt`:
```cmake
add_library(nng_gateway_core STATIC
    telemetry_parser.cpp
)
target_link_libraries(nng_gateway_core PUBLIC nng_common)
target_include_directories(nng_gateway_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
```

Add `add_subdirectory(src/gateway)` to root CMakeLists.txt.

**test:** `tests/test_telemetry_parser.cpp`:
- Valid TRACK frame (no CRC) -> OK, all header fields correct
- Valid PLOT frame with CRC -> OK, CRC matches
- Buffer too short (10 bytes) -> TOO_SHORT
- Bad version (version=99) -> BAD_VERSION
- Bad msg_type (msg_type=0xFF) -> BAD_MSG_TYPE
- payload_len=2000 -> PAYLOAD_TOO_LONG
- Truncated payload (header says 100 bytes, buffer only has 50) -> TRUNCATED
- CRC enabled but CRC bytes are wrong -> CRC_MISMATCH
- Valid HEARTBEAT frame -> OK, payload deserializes correctly
- Valid ENGAGEMENT frame -> OK

**acceptance:**
- All 10 test cases pass
- Parser never reads beyond `buf + len`
- No undefined behavior under AddressSanitizer (`-fsanitize=address`)

---

### P1.05 -- Implement SequenceTracker (sequence_tracker.h / .cpp)

**depends:** P1.01
**files:** `src/gateway/sequence_tracker.h`, `src/gateway/sequence_tracker.cpp`,
`tests/test_sequence_tracker.cpp`

Tracks per-source sequence numbers and detects anomalies.

```cpp
namespace nng {

enum class SeqResult {
    FIRST,      // first frame from this source
    OK,         // seq == expected (previous + 1)
    GAP,        // seq > expected (frames missing)
    REORDER,    // seq < expected but not duplicate
    DUPLICATE,  // seq already seen
};

struct SeqEvent {
    SeqResult result;
    uint16_t  src_id;
    uint32_t  expected_seq;
    uint32_t  actual_seq;
    uint32_t  gap_size;     // only meaningful for GAP
};

class SequenceTracker {
public:
    // Feed a frame's src_id and seq, get back what happened
    SeqEvent track(uint16_t src_id, uint32_t seq);

    // Reset state for a specific source or all sources
    void reset(uint16_t src_id);
    void reset_all();

    // Query: how many sources are being tracked
    size_t source_count() const;
};

} // namespace nng
```

Internal state: `std::unordered_map<uint16_t, SourceState>` where `SourceState`
holds `next_expected_seq` and a small sliding window (e.g., bitset of last 64
seq numbers) for duplicate detection.

**test:** `tests/test_sequence_tracker.cpp`:
- First frame from src=1 -> FIRST
- Sequential frames (seq 0,1,2,3) -> OK,OK,OK
- Gap: seq 0,1,5 -> OK, GAP with gap_size=3
- Reorder: seq 0,1,2,5,3 -> ..., GAP, REORDER
- Duplicate: seq 0,1,2,2 -> ..., DUPLICATE
- Multiple sources: src=1 seq=0,1; src=2 seq=0,1 -> independent tracking
- Reset: after reset(src=1), next frame is FIRST again
- Large gap: seq 0, 1000 -> GAP with gap_size=999

**acceptance:**
- All 8 test cases pass
- No heap allocation per `track()` call on the hot path (map lookup only)

---

### P1.06 -- Implement Logger (logger.h / logger.cpp)

**depends:** P1.01
**files:** `src/common/logger.h`, `src/common/logger.cpp`,
`tests/test_logger.cpp`

Structured logger that outputs lines matching the format in ARCHITECTURE.md
section 3.3.

```cpp
namespace nng {

class Logger {
public:
    // Set minimum severity (messages below this are dropped)
    void set_level(Severity level);
    Severity get_level() const;

    // Set output stream (default: stdout)
    void set_output(std::ostream& os);

    // Log a structured message
    // Example: log(Severity::INFO, EventCategory::TRACKING, "EVT_TRACK_NEW",
    //              "src=0x0012 track_id=1041 class=UAV_SMALL az=045.2")
    void log(Severity sev, EventCategory cat,
             const std::string& event_name,
             const std::string& detail);

    // Convenience: get singleton instance
    static Logger& instance();
};

} // namespace nng
```

Output format (one line per call):
```
2025-07-15T14:23:01.001Z [INFO ] [TRACKING  ] EVT_TRACK_NEW       src=0x0012 ...
```

Fields:
- ISO 8601 timestamp with milliseconds
- Severity padded to 5 chars in brackets
- Category padded to 10 chars in brackets
- Event name padded to 20 chars
- Free-form detail string

**test:** `tests/test_logger.cpp`:
- Log an INFO message to a `std::ostringstream`, verify format matches pattern
- Set level to WARN, log an INFO message -> nothing written
- Set level to DEBUG, log a DEBUG message -> written
- Verify timestamp is valid ISO 8601
- Verify category and severity padding

**acceptance:**
- All 5 test cases pass
- Logger is thread-safe (uses `std::mutex` internally)
- Format exactly matches ARCHITECTURE.md section 3.3 example

---

### P1.07 -- Implement EventBus (event_bus.h / event_bus.cpp)

**depends:** P1.01
**files:** `src/common/event_bus.h`, `src/common/event_bus.cpp`,
`tests/test_event_bus.cpp`

Simple synchronous publish/subscribe for internal events.

```cpp
namespace nng {

struct EventRecord {
    EventId       id;
    EventCategory category;
    Severity      severity;
    uint64_t      timestamp_ns;  // steady_clock or system_clock
    std::string   detail;        // free-form key=value pairs
};

class EventBus {
public:
    using Callback = std::function<void(const EventRecord&)>;

    // Subscribe to all events in a category. Returns subscription ID.
    uint32_t subscribe(EventCategory cat, Callback cb);

    // Subscribe to all events regardless of category.
    uint32_t subscribe_all(Callback cb);

    // Unsubscribe by ID
    void unsubscribe(uint32_t sub_id);

    // Publish an event (calls subscribers synchronously)
    void publish(const EventRecord& event);
};

} // namespace nng
```

**test:** `tests/test_event_bus.cpp`:
- Subscribe to TRACKING, publish TRACKING event -> callback fires
- Subscribe to TRACKING, publish NETWORK event -> callback does NOT fire
- subscribe_all -> receives events from any category
- Unsubscribe -> callback no longer fires
- Multiple subscribers on same category -> all fire
- Publish with no subscribers -> no crash

**acceptance:**
- All 6 test cases pass
- Thread-safe (mutex-protected subscriber list)

---

### P1.08 -- Implement StatsManager (stats_manager.h / .cpp)

**depends:** P1.01
**files:** `src/gateway/stats_manager.h`, `src/gateway/stats_manager.cpp`,
`tests/test_stats_manager.cpp`

Maintains global and per-source counters.

```cpp
namespace nng {

struct GlobalStats {
    uint64_t rx_total       = 0;
    uint64_t malformed_total= 0;
    uint64_t gap_total      = 0;
    uint64_t reorder_total  = 0;
    uint64_t duplicate_total= 0;
    uint64_t crc_fail_total = 0;
};

struct SourceStats {
    uint16_t src_id         = 0;
    uint64_t rx_count       = 0;
    uint64_t malformed      = 0;
    uint64_t gaps           = 0;
    uint64_t reorders       = 0;
    uint64_t duplicates     = 0;
    uint32_t last_seq       = 0;
    uint64_t last_ts_ns     = 0;
};

enum class HealthState { OK, DEGRADED, ERROR };

class StatsManager {
public:
    void record_rx(uint16_t src_id, uint32_t seq, uint64_t ts_ns);
    void record_malformed(uint16_t src_id);
    void record_gap(uint16_t src_id, uint32_t gap_size);
    void record_reorder(uint16_t src_id);
    void record_duplicate(uint16_t src_id);
    void record_crc_fail(uint16_t src_id);

    GlobalStats get_global_stats() const;
    SourceStats get_source_stats(uint16_t src_id) const;
    std::vector<SourceStats> get_all_source_stats() const;

    // Health: OK if no errors in last N seconds, DEGRADED if gaps > threshold, etc.
    HealthState get_health() const;

    void reset();
};

} // namespace nng
```

**test:** `tests/test_stats_manager.cpp`:
- `record_rx` 10 times -> `rx_total == 10`
- `record_rx` for two different src_ids -> per-source counts correct
- `record_gap` increments both global and per-source gap counters
- `record_malformed` increments malformed counters
- All counters start at zero
- `reset()` clears everything
- `get_health()` returns OK when clean, DEGRADED after gaps

**acceptance:**
- All 7 test cases pass
- Thread-safe reads (`std::shared_mutex`: readers for GET, writer for record_*)

---

### P1.09 -- CMake: wire up all Phase 1 libraries and tests

**depends:** P1.03, P1.04, P1.05, P1.06, P1.07, P1.08
**files:** `CMakeLists.txt`, `src/common/CMakeLists.txt`,
`src/gateway/CMakeLists.txt`, `tests/CMakeLists.txt`

Update CMake to:
- `nng_common` STATIC library: `crc32.cpp`, `logger.cpp`, `event_bus.cpp`
- `nng_gateway_core` STATIC library: `telemetry_parser.cpp`,
  `sequence_tracker.cpp`, `stats_manager.cpp`
- All test executables linked against `gtest_main`
- `add_test()` for each test executable
- `ctest` runs all tests

**test:** `cd build && cmake .. && cmake --build . && ctest --output-on-failure`
**acceptance:**
- Build succeeds with zero warnings (`-Wall -Wextra`)
- All tests pass (at least 46 test cases across 6 test files)

---

## Phase 2: Sensor Simulation (In-Memory, No Sockets)

### P2.01 -- Implement ObjectGenerator (object_generator.h / .cpp)

**depends:** P1.01
**files:** `src/sensor_sim/object_generator.h`,
`src/sensor_sim/object_generator.cpp`,
`src/sensor_sim/CMakeLists.txt`,
`tests/test_object_generator.cpp`

Generates random `WorldObject` instances based on a scenario profile.

```cpp
namespace nng {

struct WorldObject {
    uint32_t   id;
    TrackClass classification;
    double     spawn_time_s;
    double     lifetime_s;
    double     azimuth_deg;    // current position
    double     elevation_deg;
    double     range_m;
    double     speed_mps;
    double     heading_deg;
    double     rcs_dbsm;
    bool       is_hostile;
    double     noise_stddev;
};

struct ScenarioProfile {
    std::string name;
    int         min_objects;
    int         max_objects;
    std::vector<TrackClass> allowed_types;
    double      spawn_rate_hz;    // new objects per second
    double      min_range_m;
    double      max_range_m;
    double      min_speed_mps;
    double      max_speed_mps;
    double      hostile_probability;  // 0.0 to 1.0
};

// Predefined profiles
ScenarioProfile profile_idle();
ScenarioProfile profile_patrol();
ScenarioProfile profile_raid();
ScenarioProfile profile_stress();

class ObjectGenerator {
public:
    explicit ObjectGenerator(const ScenarioProfile& profile, uint32_t seed = 42);

    // Generate initial batch of objects
    std::vector<WorldObject> generate_initial();

    // Maybe spawn a new object (call once per tick)
    // Returns empty optional if nothing spawned
    std::optional<WorldObject> maybe_spawn(double current_time_s);

private:
    // uses std::mt19937 with the given seed
};

} // namespace nng
```

Create `src/sensor_sim/CMakeLists.txt`:
```cmake
add_library(nng_sensor_sim STATIC
    object_generator.cpp
)
target_link_libraries(nng_sensor_sim PUBLIC nng_common)
```

**test:** `tests/test_object_generator.cpp`:
- `profile_idle()` -> `generate_initial()` returns 0-2 objects
- `profile_raid()` -> `generate_initial()` returns 10-30 objects
- All generated objects have valid `classification` from profile's allowed_types
- `range_m` is within profile's `[min_range_m, max_range_m]`
- Deterministic: same seed -> same objects
- `maybe_spawn` with high spawn_rate returns objects frequently
- `maybe_spawn` with zero spawn_rate returns nothing

**acceptance:**
- All 7 test cases pass
- Deterministic with fixed seed

---

### P2.02 -- Implement WorldModel (world_model.h / .cpp)

**depends:** P2.01
**files:** `src/sensor_sim/world_model.h`, `src/sensor_sim/world_model.cpp`,
`tests/test_world_model.cpp`

Maintains the set of active objects, advances positions, removes expired objects.

```cpp
namespace nng {

class WorldModel {
public:
    // Add an object to the world
    void add_object(WorldObject obj);

    // Advance simulation by dt seconds
    // - updates positions (linear motion along heading)
    // - removes objects past their lifetime
    // - returns list of currently active objects
    const std::vector<WorldObject>& tick(double dt, double current_time_s);

    // How many objects are active
    size_t active_count() const;

    // Get active objects (const ref)
    const std::vector<WorldObject>& objects() const;
};

} // namespace nng
```

Position update per tick:
- `range_m += speed_mps * cos(heading_rad) * dt` (simplified radial)
- `azimuth_deg += (speed_mps * sin(heading_rad) * dt) / range_m * (180/pi)`
- Object removed when `current_time_s > spawn_time_s + lifetime_s`
- Object removed when `range_m < 50` (passed through origin)

**test:** `tests/test_world_model.cpp`:
- Add one object, tick(1.0) -> position changes
- Object with lifetime=5s, tick past 5s -> removed
- Object moving inward (negative radial velocity) reaches range < 50 -> removed
- Add 3 objects, tick -> all positions update independently
- Empty world, tick -> no crash, returns empty list

**acceptance:**
- All 5 test cases pass
- No floating point exceptions

---

### P2.03 -- Implement MeasurementGenerator (measurement_generator.h / .cpp)

**depends:** P1.02, P2.02
**files:** `src/sensor_sim/measurement_generator.h`,
`src/sensor_sim/measurement_generator.cpp`,
`tests/test_measurement_generator.cpp`

Converts world objects into serialized TelemetryFrame byte buffers (PLOT and
TRACK messages).

```cpp
namespace nng {

class MeasurementGenerator {
public:
    explicit MeasurementGenerator(uint16_t src_id, uint32_t seed = 123);

    // Given active objects, produce PLOT frames (raw detections with noise)
    // Each object may or may not be detected (probability based on RCS/range)
    std::vector<std::vector<uint8_t>> generate_plots(
        const std::vector<WorldObject>& objects, uint64_t timestamp_ns);

    // Given active objects, produce TRACK frames (associated detections)
    std::vector<std::vector<uint8_t>> generate_tracks(
        const std::vector<WorldObject>& objects, uint64_t timestamp_ns);

    // Produce a HEARTBEAT frame
    std::vector<uint8_t> generate_heartbeat(uint64_t timestamp_ns);

    // Produce an ENGAGEMENT_STATUS frame for a given weapon
    std::vector<uint8_t> generate_engagement(
        uint16_t weapon_id, WeaponMode mode, uint32_t assigned_track,
        uint16_t rounds, int16_t barrel_temp, uint16_t bursts,
        uint64_t timestamp_ns);
};

} // namespace nng
```

Detection probability model (simple):
- `p_detect = clamp(0.1, 1.0, rcs_linear / (range_m / 1000.0)^2)`
- Add Gaussian noise with `noise_stddev` to azimuth, elevation, range

Each output `vector<uint8_t>` is a complete serialized frame (header + payload),
ready to send as a UDP datagram. Sequence numbers auto-increment per call.

**test:** `tests/test_measurement_generator.cpp`:
- `generate_tracks` with one object -> returns exactly one frame
- Frame can be parsed by `parse_frame()` -> OK
- Parsed header has correct `msg_type`, `src_id`
- Sequence numbers increment across calls
- `generate_heartbeat` produces a parseable HEARTBEAT frame
- `generate_engagement` produces a parseable ENGAGEMENT frame
- `generate_plots` with far-away low-RCS object -> may return 0 (not detected)

**acceptance:**
- All 7 test cases pass
- Generated frames round-trip through parser cleanly

---

### P2.04 -- Implement FaultInjector (fault_injector.h / .cpp)

**depends:** P1.01
**files:** `src/sensor_sim/fault_injector.h`,
`src/sensor_sim/fault_injector.cpp`,
`tests/test_fault_injector.cpp`

Takes a vector of frames and applies configurable faults.

```cpp
namespace nng {

struct FaultConfig {
    double loss_pct     = 0.0;  // 0-100, percentage of frames to drop
    double reorder_pct  = 0.0;  // 0-100, percentage of frames to swap with neighbor
    double duplicate_pct= 0.0;  // 0-100, percentage of frames to duplicate
    double corrupt_pct  = 0.0;  // 0-100, percentage of frames to corrupt (flip random byte)
};

class FaultInjector {
public:
    explicit FaultInjector(const FaultConfig& config, uint32_t seed = 99);

    // Apply faults to a batch of frames IN PLACE
    // May remove frames (loss), swap adjacent frames (reorder),
    // duplicate frames, or corrupt bytes
    void apply(std::vector<std::vector<uint8_t>>& frames);

    // Stats for the last apply() call
    struct FaultStats {
        uint32_t dropped   = 0;
        uint32_t reordered = 0;
        uint32_t duplicated= 0;
        uint32_t corrupted = 0;
    };
    FaultStats last_stats() const;
};

} // namespace nng
```

**test:** `tests/test_fault_injector.cpp`:
- `loss_pct=0` -> no frames dropped
- `loss_pct=100` -> all frames dropped
- `loss_pct=50` with 1000 frames and fixed seed -> ~500 dropped (within 10%)
- `reorder_pct=100` -> some frames change position
- `duplicate_pct=50` with 100 frames -> output has more frames than input
- `corrupt_pct=100` -> at least one byte differs in each corrupted frame
- All faults at 0 -> output identical to input
- Deterministic: same seed, same input -> same output

**acceptance:**
- All 8 test cases pass
- Deterministic with fixed seed

---

### P2.05 -- In-memory integration test: generate -> parse -> track -> stats

**depends:** P2.03, P2.04, P1.04, P1.05, P1.08
**files:** `tests/test_pipeline_integration.cpp`

End-to-end test that wires together all Phase 1 + Phase 2 components without
any sockets.

Test scenario:
1. Create `ObjectGenerator` with `profile_patrol()`, seed=42
2. Create `WorldModel`, add initial objects
3. Run 100 ticks (dt=0.02s each = 50 Hz)
4. Each tick: `WorldModel::tick()` -> `MeasurementGenerator::generate_tracks()` ->
   `FaultInjector::apply()` -> for each frame: `parse_frame()` ->
   `SequenceTracker::track()` -> `StatsManager::record_*`
5. After all ticks, verify:
   - `rx_total > 0`
   - If faults injected: `gap_total > 0` or `malformed_total > 0`
   - Per-source stats exist for the expected src_id
   - No crashes, no sanitizer errors

**test:** Single GoogleTest case
**acceptance:**
- Test passes
- `rx_total` roughly equals `(active_objects * 100) - dropped`
- Runs under AddressSanitizer without errors

---

### P2.06 -- Scenario profile JSON loading

**depends:** P2.01
**files:** `src/sensor_sim/scenario_loader.h`,
`src/sensor_sim/scenario_loader.cpp`,
`scenarios/idle.json`, `scenarios/patrol.json`,
`scenarios/raid.json`, `scenarios/stress.json`,
`tests/test_scenario_loader.cpp`

Simple JSON parser (hand-rolled or use nlohmann/json via FetchContent) to load
`ScenarioProfile` from a `.json` file.

Example `patrol.json`:
```json
{
    "name": "patrol",
    "min_objects": 3,
    "max_objects": 8,
    "allowed_types": ["FIXED_WING", "ROTARY_WING", "UAV_SMALL"],
    "spawn_rate_hz": 0.1,
    "min_range_m": 5000,
    "max_range_m": 30000,
    "min_speed_mps": 50,
    "max_speed_mps": 300,
    "hostile_probability": 0.3
}
```

```cpp
namespace nng {
    ScenarioProfile load_scenario(const std::string& json_path);
}
```

**test:** `tests/test_scenario_loader.cpp`:
- Load `patrol.json` -> fields match expected values
- Load `raid.json` -> fields match
- Load nonexistent file -> throws `std::runtime_error`
- Load malformed JSON -> throws `std::runtime_error`

**acceptance:**
- All 4 test cases pass
- JSON library fetched via CMake FetchContent (nlohmann/json recommended)

---

## Phase 3: Network Layer

### P3.01 -- Implement TcpFramer (tcp_framer.h / .cpp)

**depends:** P1.01
**files:** `src/control_node/tcp_framer.h`, `src/control_node/tcp_framer.cpp`,
`src/control_node/CMakeLists.txt`, `tests/test_tcp_framer.cpp`

Length-prefix framing for TCP: `[u32_be length][payload bytes...]`

```cpp
namespace nng {

class TcpFramer {
public:
    // Encode: prepend 4-byte big-endian length to payload
    static std::vector<uint8_t> encode(const std::string& payload);
    static std::vector<uint8_t> encode(const uint8_t* data, size_t len);

    // Decode: feed bytes incrementally, extract complete frames
    // Returns number of complete frames extracted
    void feed(const uint8_t* data, size_t len);

    // Check if a complete frame is available
    bool has_frame() const;

    // Pop the next complete frame (payload only, no length prefix)
    std::string pop_frame();

    // Reset internal buffer
    void reset();
};

} // namespace nng
```

**test:** `tests/test_tcp_framer.cpp`:
- Encode "GET HEALTH" -> 4 bytes length (big-endian 10) + 10 bytes payload
- Feed a complete frame -> `has_frame()` true, `pop_frame()` returns payload
- Feed frame in 1-byte chunks -> still assembles correctly
- Feed two frames at once -> both extractable via `pop_frame()` calls
- Feed partial frame (just length, no payload yet) -> `has_frame()` false
- Feed remaining bytes -> `has_frame()` true
- Empty payload -> works (length = 0)
- Very large payload (64KB) -> works

**acceptance:**
- All 8 test cases pass
- No buffer overflows under AddressSanitizer

---

### P3.02 -- Implement CommandHandler (command_handler.h / .cpp)

**depends:** P1.08, P1.06
**files:** `src/control_node/command_handler.h`,
`src/control_node/command_handler.cpp`,
`tests/test_command_handler.cpp`

Parses ASCII commands and produces responses. Does not handle sockets -- just
string in, string out.

```cpp
namespace nng {

class CommandHandler {
public:
    explicit CommandHandler(StatsManager& stats, Logger& logger);

    // Process a command string, return a response string
    std::string handle(const std::string& command);

private:
    StatsManager& stats_;
    Logger& logger_;
    std::unordered_map<std::string, std::string> config_;
};

} // namespace nng
```

Supported commands:
- `GET HEALTH` -> returns `"HEALTH OK"` or `"HEALTH DEGRADED"` or `"HEALTH ERROR"`
- `GET STATS` -> returns multi-line stats summary
- `SET <key>=<value>` -> stores in config map, returns `"OK <key>=<value>"`
- `SET LOG_LEVEL=<level>` -> also calls `logger.set_level()`, returns `"OK LOG_LEVEL=<level>"`
- `SET CRC=ON|OFF` -> returns `"OK CRC=ON"` or `"OK CRC=OFF"`
- Unknown command -> returns `"ERR UNKNOWN_COMMAND"`

**test:** `tests/test_command_handler.cpp`:
- `"GET HEALTH"` -> response starts with `"HEALTH "`
- `"GET STATS"` -> response contains `"rx_total"`
- `"SET LOG_LEVEL=DEBUG"` -> `"OK LOG_LEVEL=DEBUG"`, logger level changed
- `"SET CRC=ON"` -> `"OK CRC=ON"`
- `"SET FOO=BAR"` -> `"OK FOO=BAR"`
- `"NONSENSE"` -> `"ERR UNKNOWN_COMMAND"`
- `"GET"` (incomplete) -> `"ERR UNKNOWN_COMMAND"`

**acceptance:**
- All 7 test cases pass
- Case-sensitive matching (commands are uppercase)

---

### P3.03 -- Implement UdpFrameSource (udp abstraction)

**depends:** P1.02, P1.04
**files:** `src/gateway/frame_source.h`, `src/gateway/udp_frame_source.h`,
`src/gateway/udp_frame_source.cpp`

```cpp
namespace nng {

class IFrameSource {
public:
    virtual ~IFrameSource() = default;
    virtual bool receive(std::vector<uint8_t>& buf) = 0;
};

class UdpFrameSource : public IFrameSource {
public:
    // Bind to a UDP port
    bool bind(uint16_t port);

    // Receive one datagram (non-blocking or with timeout_ms)
    bool receive(std::vector<uint8_t>& buf) override;

    // Close socket
    void close();

    // Set receive timeout in milliseconds
    void set_timeout_ms(int ms);

private:
    int sockfd_ = -1;
};

} // namespace nng
```

Also create an `IFrameSink` for the sender side:

```cpp
class IFrameSink {
public:
    virtual ~IFrameSink() = default;
    virtual bool send(const std::vector<uint8_t>& buf) = 0;
};

class UdpFrameSink : public IFrameSink {
public:
    bool connect(const std::string& host, uint16_t port);
    bool send(const std::vector<uint8_t>& buf) override;
    void close();
private:
    int sockfd_ = -1;
};
```

**test:** No automated unit test (requires loopback socket). Test in P3.04.
**acceptance:**
- Compiles cleanly
- Uses POSIX `socket()`, `bind()`, `recvfrom()`, `sendto()`
- Handles `EAGAIN`/`EWOULDBLOCK` correctly
- `close()` calls `::close(sockfd_)` safely

---

### P3.04 -- Loopback integration test: sensor_sim -> gateway over UDP

**depends:** P3.03, P2.03, P1.04, P1.05, P1.08
**files:** `tests/test_udp_loopback.cpp`

Spawn a sender thread and receiver thread on `127.0.0.1:15000`.

- Sender: generate 100 TRACK frames via `MeasurementGenerator`, send via
  `UdpFrameSink`
- Receiver: `UdpFrameSource` -> `parse_frame()` -> `SequenceTracker::track()` ->
  `StatsManager::record_rx()`
- After sender is done, wait 1 second, then check:
  - `rx_total >= 90` (allow some OS-level loss on loopback, unlikely but safe)
  - No crashes

**test:** Single GoogleTest case
**acceptance:**
- Test passes on Linux
- Runs in < 5 seconds

---

### P3.05 -- Implement ControlNode TCP server

**depends:** P3.01, P3.02
**files:** `src/control_node/control_node.h`,
`src/control_node/control_node.cpp`

TCP server that:
- Listens on a configured port
- Accepts up to N simultaneous clients
- Reads length-prefixed frames via `TcpFramer`
- Dispatches to `CommandHandler`
- Sends length-prefixed response back
- Runs in a dedicated thread

```cpp
namespace nng {

class ControlNode {
public:
    ControlNode(uint16_t port, StatsManager& stats, Logger& logger);

    // Start listening (spawns accept thread)
    bool start();

    // Stop (closes all connections, joins thread)
    void stop();

    bool is_running() const;
};

} // namespace nng
```

**test:** No automated unit test for the TCP server itself (covered in P3.06).
**acceptance:**
- Compiles cleanly
- Handles client disconnect without crashing
- `stop()` cleanly shuts down within 2 seconds

---

### P3.06 -- Implement CLI client + TCP loopback test

**depends:** P3.05
**files:** `src/cli/cli_client.h`, `src/cli/cli_client.cpp`,
`src/cli/cli_main.cpp`, `tests/test_tcp_loopback.cpp`

`CliClient`:
```cpp
namespace nng {

class CliClient {
public:
    bool connect(const std::string& host, uint16_t port);
    std::string send_command(const std::string& cmd);
    void close();
};

} // namespace nng
```

`cli_main.cpp`: simple REPL that reads lines from stdin, sends them, prints
responses.

Loopback test: start `ControlNode` on port 16000, connect `CliClient`,
send `"GET HEALTH"`, verify response starts with `"HEALTH "`.

**test:** `tests/test_tcp_loopback.cpp`
**acceptance:**
- Loopback test passes
- CLI compiles and links as executable `cli`

---

## Phase 4: Record / Replay

### P4.01 -- Implement FrameRecorder (frame_recorder.h / .cpp)

**depends:** P1.02
**files:** `src/gateway/frame_recorder.h`, `src/gateway/frame_recorder.cpp`,
`tests/test_frame_recorder.cpp`

Records received frames to a binary file.

File format:
```
For each recorded frame:
  [uint64_t  rx_timestamp_ns]  // when frame was received (steady_clock)
  [uint32_t  frame_length]     // length of the raw frame bytes
  [uint8_t[] frame_bytes]      // the raw datagram
```

```cpp
namespace nng {

class FrameRecorder {
public:
    // Open file for writing
    bool open(const std::string& path);

    // Record one frame with its receive timestamp
    bool record(uint64_t rx_timestamp_ns,
                const uint8_t* frame_data, size_t frame_len);

    // Close file
    void close();

    // How many frames recorded so far
    uint64_t frame_count() const;
};

} // namespace nng
```

**test:** `tests/test_frame_recorder.cpp`:
- Record 5 frames to a temp file -> file exists, size > 0
- `frame_count()` == 5 after recording 5 frames
- Close and reopen file for reading: verify correct number of frame entries
  and frame contents match
- Record with closed file -> returns false
- Record empty frame (len=0) -> works

**acceptance:**
- All 5 test cases pass
- File can be read back correctly (verified in P4.02)

---

### P4.02 -- Implement ReplayFrameSource (replay_engine.h / .cpp)

**depends:** P4.01, P3.03
**files:** `src/replay/replay_engine.h`, `src/replay/replay_engine.cpp`,
`tests/test_replay_engine.cpp`

Reads a recorded file and replays frames, implementing `IFrameSource`.

```cpp
namespace nng {

class ReplayFrameSource : public IFrameSource {
public:
    // Open recorded file
    bool open(const std::string& path);

    // Set playback speed multiplier (1.0 = real-time, 0.0 = as fast as possible)
    void set_speed(double multiplier);

    // IFrameSource interface
    bool receive(std::vector<uint8_t>& buf) override;

    // Is there more data?
    bool is_done() const;

    // How many frames replayed so far
    uint64_t frames_replayed() const;

    void close();
};

} // namespace nng
```

**test:** `tests/test_replay_engine.cpp`:
- Record 10 frames via `FrameRecorder`, replay via `ReplayFrameSource` ->
  same 10 frames come back in same order with same content
- `is_done()` true after all frames replayed
- `frames_replayed()` == 10
- Speed 0.0 (fast) -> all frames returned immediately
- Open nonexistent file -> returns false

**acceptance:**
- All 5 test cases pass
- Record + replay produces byte-identical frames

---

### P4.03 -- Replay determinism integration test

**depends:** P4.02, P2.05
**files:** `tests/test_replay_determinism.cpp`

Full pipeline test:
1. Run in-memory pipeline (Phase 2 style), record all frames via `FrameRecorder`
2. Replay recorded frames via `ReplayFrameSource`
3. Feed replayed frames through same parser -> tracker -> stats pipeline
4. Compare `GlobalStats` from live run vs replay run -> must be identical

**test:** Single GoogleTest case
**acceptance:**
- `rx_total`, `gap_total`, `reorder_total`, `malformed_total` all match
  between live and replay runs

---

### P4.04 -- Replay tool CLI (replay_main.cpp)

**depends:** P4.02
**files:** `src/replay/replay_main.cpp`

CLI executable that:
- Takes arguments: `--file <path>` `--speed <multiplier>` `--port <udp_port>`
- Opens the recorded file
- Replays frames via `UdpFrameSink` to the given port (or just to stdout with
  `--dry-run` printing frame summaries)
- Prints summary at the end: frames replayed, duration, effective rate

**test:** Manual: `./replay --file recorded/test.bin --dry-run`
**acceptance:**
- Compiles and links as executable `replay`
- `--dry-run` prints one line per frame with seq, src_id, msg_type

---

## Phase 5: Gateway Main + Full Integration

### P5.01 -- Implement Gateway main loop (gateway.h / .cpp)

**depends:** P3.03, P1.04, P1.05, P1.07, P1.08, P4.01
**files:** `src/gateway/gateway.h`, `src/gateway/gateway.cpp`,
`src/gateway/gateway_main.cpp`

The gateway main loop:
1. Create `IFrameSource` (UDP or Replay based on config)
2. Loop: `receive()` -> `parse_frame()` -> `sequence_tracker.track()` ->
   `stats_manager.record_*()` -> `event_bus.publish()` ->
   `frame_recorder.record()` (if enabled)
3. Log events via `Logger`

```cpp
namespace nng {

struct GatewayConfig {
    uint16_t udp_port        = 5000;
    bool     crc_enabled     = true;
    bool     record_enabled  = false;
    std::string record_path  = "./recorded/session.bin";
    std::string replay_path;  // if non-empty, use replay instead of UDP
    Severity log_level       = Severity::INFO;
};

class Gateway {
public:
    explicit Gateway(const GatewayConfig& config);

    // Run the main loop (blocking, until stop() is called from another thread)
    void run();
    void stop();

    // Accessors for control node integration
    StatsManager& stats();
    EventBus& events();
    Logger& logger();
};

} // namespace nng
```

`gateway_main.cpp`: parse CLI args, create `Gateway`, `run()`.

**test:** Covered by P5.03
**acceptance:**
- Compiles and links as executable `gateway`
- Accepts `--port`, `--crc`, `--record`, `--replay`, `--log-level` arguments

---

### P5.02 -- Implement sensor_sim_main.cpp

**depends:** P2.01, P2.02, P2.03, P2.04, P3.03
**files:** `src/sensor_sim/sensor_sim_main.cpp`

Entry point for the sensor simulator:
1. Parse CLI args: `--profile <name|json_path>` `--host <ip>` `--port <udp_port>`
   `--rate <hz>` `--duration <seconds>` `--seed <int>` `--loss <pct>`
   `--reorder <pct>` `--duplicate <pct>`
2. Create `ObjectGenerator`, `WorldModel`, `MeasurementGenerator`, `FaultInjector`
3. Create `UdpFrameSink` connected to gateway
4. Loop at configured rate:
   - `world_model.tick()`
   - `generate_tracks()` + `generate_plots()` + occasional `generate_heartbeat()`
   - `fault_injector.apply()`
   - Send each frame via `UdpFrameSink`
5. Print summary at end: frames sent, faults injected

**test:** Manual: `./sensor_sim --profile patrol --port 5000 --duration 5`
**acceptance:**
- Compiles and links as executable `sensor_sim`
- Produces structured log output showing frames sent
- Prints summary at exit

---

### P5.03 -- Full system integration test

**depends:** P5.01, P5.02, P3.05
**files:** `tests/test_full_system.cpp`

Automated integration test:
1. Start `Gateway` in a thread on UDP port 17000, recording enabled
2. Start `ControlNode` in a thread on TCP port 17001
3. Start sensor sim in a thread: `profile_patrol`, sending to UDP 17000,
   duration 3 seconds
4. Wait for sensor sim to finish + 1 second drain
5. Query stats via `CliClient` -> `"GET STATS"` -> verify `rx_total > 0`
6. Query health -> `"GET HEALTH"` -> response is `"HEALTH OK"` or `"HEALTH DEGRADED"`
7. Stop gateway and control node
8. Verify recorded file exists and has size > 0

**test:** Single GoogleTest case
**acceptance:**
- Test passes on Linux
- `rx_total > 50` (patrol sends many frames in 3 seconds)
- Recorded file is non-empty
- No crashes, no sanitizer errors
- Runs in < 10 seconds

---

### P5.04 -- Full system with fault injection integration test

**depends:** P5.03
**files:** `tests/test_full_system_faults.cpp`

Same as P5.03 but with `loss_pct=5`, `reorder_pct=2`, `duplicate_pct=1`.

Additional checks:
- `gap_total > 0` (some frames were lost)
- `reorder_total > 0` (some frames were reordered)
- Gateway did not crash

**test:** Single GoogleTest case
**acceptance:**
- Test passes
- Fault counters are non-zero
- `rx_total < frames_sent` (due to loss)

---

### P5.05 -- Structured log output verification

**depends:** P5.01, P1.06
**files:** `tests/test_log_format.cpp`

Verify that the gateway's log output matches the format defined in
ARCHITECTURE.md section 3.3.

Run a short in-memory pipeline, capture logger output to a `std::ostringstream`,
then verify each line matches the regex:

```
^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z \[(DEBUG|INFO |WARN |ALARM|ERROR|FATAL)\] \[.{10}\] .{20}.+$
```

**test:** `tests/test_log_format.cpp`
**acceptance:**
- All logged lines match the expected format
- At least 5 different event types appear in the output

---

### P5.06 -- Record + replay + stats comparison (end-to-end)

**depends:** P5.03, P4.03
**files:** `tests/test_e2e_replay.cpp`

1. Run full system (P5.03 style), record to file
2. Replay recorded file through gateway (replay mode)
3. Compare `GlobalStats`: `rx_total` must match, `gap_total` must match,
   `malformed_total` must match
4. This validates NFR-2 (determinism)

**test:** Single GoogleTest case
**acceptance:**
- All counter values match between live and replay runs
- This is the definitive determinism proof

---

## Summary

| Phase | Tasks | Key Deliverable |
|-------|-------|-----------------|
| P0 | 2 | Project scaffolding, CMake + GoogleTest |
| P1 | 9 | Core logic: protocol, CRC, parser, tracker, logger, events, stats |
| P2 | 6 | Sensor sim: objects, world, measurements, faults, integration |
| P3 | 6 | Network: TCP framer, commands, UDP source/sink, loopback tests |
| P4 | 4 | Record/replay: recorder, replay engine, determinism test |
| P5 | 6 | Full system: gateway main, sensor main, integration tests |
| **Total** | **33** | |

Each task produces 1-3 files and has explicit, testable acceptance criteria.
Tasks are ordered so that each depends only on previously completed tasks.
The Sonnet model should implement one task per session, run the tests, and
verify acceptance before moving to the next task.
