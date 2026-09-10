// SPDX-License-Identifier: GPL-3.0-only

#pragma once

namespace tagtinker::keyboard {

template <typename Keys> bool consoleEraseRequested(const Keys& keys) {
  return keys.backspace || keys.del;
}

} // namespace tagtinker::keyboard
