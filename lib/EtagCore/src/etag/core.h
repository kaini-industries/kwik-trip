#pragma once
#include <cstddef>
#include <cstdint>

namespace etag {
enum class Protocol { Unknown, TiCcDebug };
enum class Error { Ok, Unsupported, NoResponse, UnstableResponse, InvalidArgument };
const char* errorName(Error error);

struct TargetProfile {
    const char* id;
    const char* model;
    Protocol protocol;
    bool verified;
    uint8_t expectedChipId;
    uint32_t flashBytes;  // Zero means unknown, never infer from model ID alone.
};

struct ProbeResult {
    Error error = Error::NoResponse;
    uint16_t chipId = 0;
    uint8_t status = 0;
    bool locked() const { return (status & 0x04U) != 0; }
};

// Physical adapters own GPIO direction, turnaround, and reset. No Arduino dependency.
class DebugWire {
public:
    virtual ~DebugWire() = default;
    virtual void enter() = 0;
    virtual void send(uint8_t value) = 0;
    virtual uint8_t receive() = 0;
    virtual void release() = 0;
};

class CcDebugProbe {
public:
    explicit CcDebugProbe(DebugWire& wire) : wire_(wire) {}
    ProbeResult run(const TargetProfile& profile);
private:
    DebugWire& wire_;
    uint16_t readId();
};

// Utilities for future programming backends; subtraction avoids address overflow.
bool fitsRange(uint32_t address, uint32_t size, uint32_t capacity);
bool stageBlock(const uint8_t* source, size_t length, size_t offset,
                uint8_t* output, size_t outputSize, size_t& copied);

// Overlong commands are discarded as a whole, never executed as a truncated prefix.
class LineBuffer {
public:
    enum class Event { None, Ready, Overflow };
    Event push(char value);
    const char* line() const { return data_; }
private:
    char data_[96] = {};
    size_t used_ = 0;
    bool overflow_ = false;
};
}  // namespace etag
