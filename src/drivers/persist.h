#pragma once
// drivers/persist.h — 配置持久化（NVS / Preferences）

#include "state.h"

namespace persist {
void init();
void load(ice::Config& cfg);
void save(const ice::Config& cfg);
}  // namespace persist
