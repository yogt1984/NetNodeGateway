#pragma once
#include <cstdint>
#include <cstddef>

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
    UNKNOWN          = 0x00,
    FIXED_WING       = 0x01,
    ROTARY_WING      = 0x02,
    UAV_SMALL        = 0x03,
    UAV_LARGE        = 0x04,
    MISSILE          = 0x05,
    ROCKET_ARTILLERY = 0x06,
    BIRD             = 0x07,
    DECOY            = 0x08,
};

// Threat level
enum class ThreatLevel : uint8_t {
    UNKNOWN  = 0,
    LOW      = 1,
    MEDIUM   = 2,
    HIGH     = 3,
    CRITICAL = 4,
};

// IFF status
enum class IffStatus : uint8_t {
    NO_RESPONSE = 0,
    FRIEND      = 1,
    FOE         = 2,
    PENDING     = 3,
};

// Subsystem state (heartbeat)
enum class SubsystemState : uint8_t {
    OK       = 0,
    DEGRADED = 1,
    ERROR    = 2,
    OFFLINE  = 3,
};

// Weapon mode
enum class WeaponMode : uint8_t {
    SAFE       = 0,
    ARMED      = 1,
    ENGAGING   = 2,
    CEASE_FIRE = 3,
};

// Log severity
enum class Severity : uint8_t {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ALARM = 3,
    ERROR = 4,
    FATAL = 5,
};

// Event category
enum class EventCategory : uint8_t {
    TRACKING   = 0,
    THREAT     = 1,
    IFF        = 2,
    ENGAGEMENT = 3,
    NETWORK    = 4,
    HEALTH     = 5,
    CONTROL    = 6,
};

// Event ID
enum class EventId : uint16_t {
    EVT_TRACK_NEW         = 0x0100,
    EVT_TRACK_UPDATE      = 0x0101,
    EVT_TRACK_LOST        = 0x0102,
    EVT_TRACK_CLASSIFY    = 0x0103,
    EVT_THREAT_EVAL       = 0x0200,
    EVT_THREAT_CRITICAL   = 0x0201,
    EVT_IFF_RESPONSE      = 0x0300,
    EVT_IFF_FOE           = 0x0301,
    EVT_ENGAGE_START      = 0x0400,
    EVT_ENGAGE_CEASE      = 0x0401,
    EVT_WEAPON_STATUS     = 0x0402,
    EVT_AMMO_LOW          = 0x0403,
    EVT_SEQ_GAP           = 0x0500,
    EVT_SEQ_REORDER       = 0x0501,
    EVT_FRAME_MALFORMED   = 0x0502,
    EVT_CRC_FAIL          = 0x0503,
    EVT_SOURCE_ONLINE     = 0x0504,
    EVT_SOURCE_TIMEOUT    = 0x0505,
    EVT_HEARTBEAT_OK      = 0x0600,
    EVT_HEARTBEAT_DEGRADE = 0x0601,
    EVT_HEARTBEAT_ERROR   = 0x0602,
    EVT_CONFIG_CHANGE     = 0x0700,
};

// Telemetry frame header size (without payload)
// version(1) + msg_type(1) + src_id(2) + seq(4) + ts_ns(8) + payload_len(2) = 18
constexpr std::size_t FRAME_HEADER_SIZE = 18;
constexpr std::size_t FRAME_CRC_SIZE    = 4;
constexpr std::size_t MAX_PAYLOAD_SIZE  = 1024;

} // namespace nng
