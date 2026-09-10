#pragma once
#include <Arduino.h>
#include <etag/core.h>

class GpioDebugWire final : public etag::DebugWire {
public:
    GpioDebugWire(int dd, int dc, int reset) : dd_(dd), dc_(dc), reset_(reset) {}
    void idle();
    void enter() override;
    void send(uint8_t value) override;
    uint8_t receive() override;
    void release() override;
private:
    int dd_, dc_, reset_;
};
