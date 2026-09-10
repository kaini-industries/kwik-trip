#include "etag/core.h"
#include <cstring>

namespace etag {
const char* errorName(Error error) {
    switch (error) {
    case Error::Ok: return "ok";
    case Error::Unsupported: return "unsupported protocol or chip";
    case Error::NoResponse: return "no target response";
    case Error::UnstableResponse: return "inconsistent chip ID";
    case Error::InvalidArgument: return "invalid argument";
    }
    return "unknown error";
}

uint16_t CcDebugProbe::readId() {
    wire_.send(0x68);  // GET_CHIP_ID, model byte followed by silicon revision.
    const uint16_t model = wire_.receive();
    return static_cast<uint16_t>((model << 8U) | wire_.receive());
}

ProbeResult CcDebugProbe::run(const TargetProfile& profile) {
    ProbeResult result;
    if (profile.protocol != Protocol::TiCcDebug || profile.expectedChipId != 0x81) {
        result.error = Error::Unsupported;
        return result;  // Unknown profiles must not touch GPIO.
    }
    wire_.enter();
    result.chipId = readId();
    const uint16_t secondId = readId();
    wire_.send(0x34);  // READ_STATUS remains available on debug-locked CC2510.
    result.status = wire_.receive();
    wire_.release();  // Restore normal boot and release all pins on every outcome.
    if (result.chipId == 0 || result.chipId == 0xFFFF) {
        result.error = Error::NoResponse;
    } else if (result.chipId != secondId) {
        result.error = Error::UnstableResponse;
    } else if ((result.chipId >> 8U) != profile.expectedChipId) {
        result.error = Error::Unsupported;
    } else {
        result.error = Error::Ok;
    }
    return result;
}

bool fitsRange(uint32_t address, uint32_t size, uint32_t capacity) {
    return size != 0 && address < capacity && size <= capacity - address;
}

bool stageBlock(const uint8_t* source, size_t length, size_t offset,
                uint8_t* output, size_t outputSize, size_t& copied) {
    copied = 0;
    if (!source || !output || !outputSize || offset >= length) return false;
    std::memset(output, 0xFF, outputSize);
    copied = length - offset < outputSize ? length - offset : outputSize;
    std::memcpy(output, source + offset, copied);
    return true;
}

LineBuffer::Event LineBuffer::push(char value) {
    if (value == '\r') return Event::None;
    if (value == '\n') {
        const Event event = overflow_ ? Event::Overflow : used_ ? Event::Ready : Event::None;
        data_[used_] = '\0';
        used_ = 0;
        overflow_ = false;
        return event;
    }
    if (overflow_) return Event::None;
    if (value == '\b' || value == 127) {
        if (used_) --used_;
    } else if (value >= 32 && value <= 126) {
        if (used_ + 1 >= sizeof(data_)) overflow_ = true;
        else data_[used_++] = value;
    }
    return Event::None;
}
}  // namespace etag
