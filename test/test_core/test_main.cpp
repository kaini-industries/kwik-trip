#include <unity.h>
#include <etag/core.h>
#include <array>
#include <vector>
#include <cstring>

namespace {
const etag::TargetProfile cc{"lab", "test", etag::Protocol::TiCcDebug, false, 0x81, 0};
class FakeWire : public etag::DebugWire {
public:
    std::vector<uint8_t> responses;
    std::vector<uint8_t> commands;
    size_t offset = 0;
    unsigned enters = 0, releases = 0;
    void enter() override { ++enters; }
    void send(uint8_t value) override { commands.push_back(value); }
    uint8_t receive() override { return offset < responses.size() ? responses[offset++] : 0xFF; }
    void release() override { ++releases; }
};

void locked_target_is_identified_without_erase() {
    FakeWire wire;
    wire.responses = {0x81, 0x04, 0x81, 0x04, 0xA4};
    const auto r = etag::CcDebugProbe(wire).run(cc);
    TEST_ASSERT_TRUE(r.error == etag::Error::Ok);
    TEST_ASSERT_TRUE(r.locked());
    TEST_ASSERT_EQUAL_HEX16(0x8104, r.chipId);
    const uint8_t expected[] = {0x68, 0x68, 0x34};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, wire.commands.data(), 3);
    TEST_ASSERT_EQUAL(1, wire.releases);
}
void unknown_protocol_never_touches_target() {
    FakeWire wire;
    auto unknown = cc;
    unknown.protocol = etag::Protocol::Unknown;
    TEST_ASSERT_TRUE(etag::CcDebugProbe(wire).run(unknown).error == etag::Error::Unsupported);
    TEST_ASSERT_EQUAL(0, wire.enters);
    TEST_ASSERT_EQUAL(0, wire.commands.size());
}
void floating_or_missing_target_is_not_a_chip() {
    FakeWire wire;
    TEST_ASSERT_TRUE(etag::CcDebugProbe(wire).run(cc).error == etag::Error::NoResponse);
    TEST_ASSERT_EQUAL(1, wire.releases);
    wire.responses = {0, 0, 0, 0, 0}; wire.offset = 0;
    TEST_ASSERT_TRUE(etag::CcDebugProbe(wire).run(cc).error == etag::Error::NoResponse);
}
void inconsistent_response_and_wrong_chip_fail() {
    FakeWire wire;
    wire.responses = {0x81, 1, 0x81, 2, 0};
    TEST_ASSERT_TRUE(etag::CcDebugProbe(wire).run(cc).error == etag::Error::UnstableResponse);
    wire.responses = {0x91, 1, 0x91, 1, 0}; wire.offset = 0;
    TEST_ASSERT_TRUE(etag::CcDebugProbe(wire).run(cc).error == etag::Error::Unsupported);
    TEST_ASSERT_EQUAL(2, wire.releases);
}
void range_checks_do_not_overflow() {
    TEST_ASSERT_TRUE(etag::fitsRange(0, 32768, 32768));
    TEST_ASSERT_FALSE(etag::fitsRange(0, 32769, 32768));
    TEST_ASSERT_FALSE(etag::fitsRange(32768, 1, 32768));
    TEST_ASSERT_FALSE(etag::fitsRange(1, 0xFFFFFFFF, 32768));
    TEST_ASSERT_FALSE(etag::fitsRange(0, 0, 32768));
}
void short_and_odd_blocks_are_padded_without_overread() {
    for (const size_t length : {1U, 63U, 64U, 65U}) {
        std::vector<uint8_t> input(length, 0x42);
        std::array<uint8_t, 66> guard{};
        guard.front() = 0xA5; guard.back() = 0xA5;
        for (size_t offset = 0; offset < length; offset += 64) {
            size_t copied = 0;
            TEST_ASSERT_TRUE(etag::stageBlock(input.data(), length, offset, guard.data() + 1, 64, copied));
            TEST_ASSERT_EQUAL(length - offset < 64 ? length - offset : 64, copied);
            for (size_t i = 0; i < 64; ++i) TEST_ASSERT_EQUAL_HEX8(i < copied ? 0x42 : 0xFF, guard[i + 1]);
            TEST_ASSERT_EQUAL_HEX8(0xA5, guard.front());
            TEST_ASSERT_EQUAL_HEX8(0xA5, guard.back());
        }
    }
}
void invalid_block_inputs_fail() {
    uint8_t data = 0; size_t copied = 99;
    TEST_ASSERT_FALSE(etag::stageBlock(nullptr, 1, 0, &data, 1, copied));
    TEST_ASSERT_EQUAL(0, copied);
    TEST_ASSERT_FALSE(etag::stageBlock(&data, 1, 1, &data, 1, copied));
}
void oversized_commands_are_discarded() {
    etag::LineBuffer line;
    for (unsigned i = 0; i < 100; ++i) line.push('x');
    TEST_ASSERT_TRUE(line.push('\n') == etag::LineBuffer::Event::Overflow);
    for (char c : std::array<char, 6>{'p', 'i', 'n', 'x', '\b', 's'}) line.push(c);
    TEST_ASSERT_TRUE(line.push('\r') == etag::LineBuffer::Event::None);
    TEST_ASSERT_TRUE(line.push('\n') == etag::LineBuffer::Event::Ready);
    TEST_ASSERT_EQUAL_STRING("pins", line.line());
}
}
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(locked_target_is_identified_without_erase);
    RUN_TEST(unknown_protocol_never_touches_target);
    RUN_TEST(floating_or_missing_target_is_not_a_chip);
    RUN_TEST(inconsistent_response_and_wrong_chip_fail);
    RUN_TEST(range_checks_do_not_overflow);
    RUN_TEST(short_and_odd_blocks_are_padded_without_overread);
    RUN_TEST(invalid_block_inputs_fail);
    RUN_TEST(oversized_commands_are_discarded);
    return UNITY_END();
}
