#include "gpio_wire.h"
#include <driver/gpio.h>

namespace {
constexpr unsigned kHalfPeriodUs = 5;
void floatPin(int pin) {
    pinMode(pin, INPUT);
    gpio_set_pull_mode(static_cast<gpio_num_t>(pin), GPIO_FLOATING);
}
}

void GpioDebugWire::idle() {
    floatPin(dd_);
    floatPin(dc_);
    floatPin(reset_);
}

void GpioDebugWire::enter() {
    floatPin(dd_);
    digitalWrite(dc_, LOW);
    pinMode(dc_, OUTPUT);
    // Target-side pull-up is required. Open drain never drives the target rail high.
    digitalWrite(reset_, HIGH);
    pinMode(reset_, OUTPUT_OPEN_DRAIN);
    digitalWrite(reset_, LOW);
    delay(2);
    for (unsigned pulse = 0; pulse < 2; ++pulse) {
        digitalWrite(dc_, HIGH);
        delayMicroseconds(kHalfPeriodUs);
        digitalWrite(dc_, LOW);
        delayMicroseconds(kHalfPeriodUs);
    }
    delay(2);
    digitalWrite(reset_, HIGH);
    delay(2);
}

void GpioDebugWire::send(uint8_t value) {
    digitalWrite(dd_, LOW);
    pinMode(dd_, OUTPUT);
    for (unsigned bit = 0; bit < 8; ++bit) {
        digitalWrite(dd_, (value & 0x80U) ? HIGH : LOW);
        digitalWrite(dc_, HIGH);
        delayMicroseconds(kHalfPeriodUs);
        digitalWrite(dc_, LOW);
        delayMicroseconds(kHalfPeriodUs);
        value <<= 1U;
    }
}

uint8_t GpioDebugWire::receive() {
    floatPin(dd_);
    uint8_t value = 0;
    for (unsigned bit = 0; bit < 8; ++bit) {
        digitalWrite(dc_, HIGH);
        delayMicroseconds(kHalfPeriodUs);
        value = static_cast<uint8_t>((value << 1U) | (digitalRead(dd_) ? 1U : 0U));
        digitalWrite(dc_, LOW);
        delayMicroseconds(kHalfPeriodUs);
    }
    return value;
}

void GpioDebugWire::release() {
    floatPin(dd_);
    digitalWrite(dc_, LOW);
    digitalWrite(reset_, LOW);
    delay(5);
    digitalWrite(reset_, HIGH);
    delay(2);
    idle();
}
