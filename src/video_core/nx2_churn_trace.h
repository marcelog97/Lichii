// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/common_types.h"
#include "common/logging.h"

// EDEN_NX2_CHURN_TRACE: where the buffer cache's page tracking churns. Every 10 s it logs, per
// 64 KiB device region, the bytes the buffer cache uploaded, the bytes the GPU command stream
// wrote into guest memory and invalidated (MemoryManager caching: constant buffer data, inline
// uploads) and the bytes the guest CPU wrote over cached pages (gathered dirty memory).
namespace VideoCommon::Nx2ChurnTrace {

inline const bool g_enabled = std::getenv("EDEN_NX2_CHURN_TRACE") != nullptr;

enum Kind : size_t { Upload, Inner, CpuWrite, NumKinds };

struct Totals {
    u64 bytes{};
    u64 count{};
};

struct State {
    std::mutex mutex;
    std::array<std::unordered_map<u64, Totals>, NumKinds> regions;
    std::chrono::steady_clock::time_point last{std::chrono::steady_clock::now()};
};

inline State& Get() {
    static State state;
    return state;
}

inline void Report(State& state) {
    static constexpr std::array<const char*, NumKinds> names{
        "uploaded by the buffer cache", "written by the command stream (inner invalidation)",
        "written by the guest CPU over cached pages"};
    for (size_t kind = 0; kind < NumKinds; ++kind) {
        auto& map = state.regions[kind];
        std::vector<std::pair<u64, Totals>> sorted(map.begin(), map.end());
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) { return a.second.bytes > b.second.bytes; });
        u64 bytes = 0, count = 0;
        for (const auto& entry : sorted) {
            bytes += entry.second.bytes;
            count += entry.second.count;
        }
        LOG_INFO(HW_GPU, "Churn trace, {} in 10 s: {} KiB in {} ranges over {} regions",
                 names[kind], bytes / 1024, count, sorted.size());
        for (size_t i = 0; i < (std::min<size_t>)(sorted.size(), 10); ++i) {
            LOG_INFO(HW_GPU, "  region {:#x}: {} KiB in {}", sorted[i].first << 16,
                     sorted[i].second.bytes / 1024, sorted[i].second.count);
        }
        map.clear();
    }
}

inline void Note(Kind kind, u64 address, u64 size) {
    auto& state = Get();
    std::scoped_lock lock{state.mutex};
    auto& totals = state.regions[kind][address >> 16];
    totals.bytes += size;
    ++totals.count;
    const auto now = std::chrono::steady_clock::now();
    if (now - state.last >= std::chrono::seconds{10}) {
        state.last = now;
        Report(state);
    }
}

} // namespace VideoCommon::Nx2ChurnTrace
