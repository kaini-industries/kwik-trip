// SPDX-License-Identifier: GPL-3.0-only
#include "ImageCodec.hpp"

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <vector>

namespace {

std::size_t liveBytes = 0;
std::size_t allocationBudget = std::numeric_limits<std::size_t>::max();

struct alignas(std::max_align_t) AllocationHeader {
  std::size_t size;
};

} // namespace

void* operator new(const std::size_t size) {
  if (size > allocationBudget - liveBytes) {
    throw std::bad_alloc();
  }
  auto* allocation =
      static_cast<AllocationHeader*>(std::malloc(sizeof(AllocationHeader) + size));
  if (allocation == nullptr) {
    throw std::bad_alloc();
  }
  allocation->size = size;
  liveBytes += size;
  return allocation + 1;
}

void operator delete(void* pointer) noexcept {
  if (pointer != nullptr) {
    auto* allocation = static_cast<AllocationHeader*>(pointer) - 1;
    liveBytes -= allocation->size;
    std::free(allocation);
  }
}

void* operator new[](const std::size_t size) { return ::operator new(size); }
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { ::operator delete(pointer); }

int main() {
  constexpr std::size_t sourceBytes = 50000U;
  std::vector<std::uint8_t> raw(sourceBytes, 0xaa);
  for (std::size_t index = 0; index < 16U; ++index) {
    raw[index] = 0;
  }

  // This is the raw-upload heap budget that previously passed its preflight
  // and then threw when packet padding caused a second, larger allocation.
  allocationBudget = sourceBytes * 2U + 32768U;
  tagtinker::image::Encoded encoded;
  assert(tagtinker::image::encode(raw, sourceBytes * 8U, encoded));
  assert(encoded.bytes.size() == sourceBytes);
  assert(liveBytes <= allocationBudget);

  encoded = {};
  assert(liveBytes == sourceBytes);

  // Upload and SD-render callers use the same non-throwing reserve boundary.
  // A failed contiguous allocation must leave their destination empty and let
  // the caller return its normal memory error instead of terminating firmware.
  std::vector<std::uint8_t> callerBuffer;
  allocationBudget = liveBytes;
  assert(!tagtinker::image::reserveBytes(callerBuffer, 4096U));
  assert(callerBuffer.empty());
  allocationBudget = sourceBytes * 2U + 32768U;
  assert(tagtinker::image::reserveBytes(callerBuffer, 4096U));
  std::vector<std::uint8_t>().swap(callerBuffer);
  assert(liveBytes == sourceBytes);

  // Allocation failure is part of the codec's ordinary error contract.
  allocationBudget = liveBytes;
  assert(!tagtinker::image::encode(raw, sourceBytes * 8U, encoded));
  assert(encoded.bytes.empty());

  // The streaming encoder follows the same allocation contract and rejects a
  // second finish instead of appending the terminal run twice.
  tagtinker::image::RleStreamEncoder stream;
  assert(!stream.begin(sourceBytes * 8U));
  allocationBudget = sourceBytes * 2U + 32768U;
  assert(stream.begin(160U));
  assert(stream.appendRun(160U, true));
  assert(stream.finish(encoded));
  const auto first = encoded.bytes;
  assert(!stream.finish(encoded));
  assert(encoded.bytes.empty());
  assert(!first.empty());
}
