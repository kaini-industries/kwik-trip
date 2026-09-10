// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "TargetStore.hpp"

namespace tagtinker::editing {

inline target::Record preserveSavedRecord(const TargetStore& targets,
                                          const target::Record& parsed) {
  for (std::size_t index = 0; index < targets.size(); ++index) {
    const target::Record* saved = targets.get(index);
    if (saved != nullptr && saved->wirePlid == parsed.wirePlid) {
      return *saved;
    }
  }
  return parsed;
}

inline void configureSegment(target::Record& record) {
  record.profile = {target::Kind::segment, target::Color::mono, 0, 0, 0, false, false};
  record.profileOverridden = true;
}

} // namespace tagtinker::editing
