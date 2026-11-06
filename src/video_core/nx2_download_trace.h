// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/common_types.h"
#include "common/logging.h"

// EDEN_NX2_DOWNLOAD_TRACE: what the buffer cache copies back to guest memory at fences. Every
// 10 s it logs the GPU writes that queued the most bytes for download (by written range), the
// 64 KiB regions copied back the most, and the first 2000 guest CPU reads of GPU-written ranges
// (a Preflushable mark sends later GPU writes over the range to a download at every fence).
namespace VideoCommon::Nx2DownloadTrace {

inline const bool g_enabled = std::getenv("EDEN_NX2_DOWNLOAD_TRACE") != nullptr;

struct Totals {
    u64 bytes{};
    u64 count{};
};

struct State {
    std::unordered_map<u64, Totals> writes;  ///< (address << 24 | size) of the GPU write
    std::unordered_map<u64, Totals> regions; ///< 64 KiB region copied back
    std::unordered_map<u64, Totals> flushed_reads; ///< 64 KiB region of CPU reads that flushed
    std::chrono::steady_clock::time_point last{std::chrono::steady_clock::now()};
    u64 marks{};
    u64 reads_flushed{};
    u64 reads_marked{};
    u64 read_bytes_flushed{};
};

inline State& Get() {
    static State state;
    return state;
}

inline void LogTop(const char* title, const std::unordered_map<u64, Totals>& map, bool write_keys) {
    std::vector<std::pair<u64, Totals>> sorted(map.begin(), map.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second.bytes > b.second.bytes; });
    u64 total = 0;
    for (const auto& entry : sorted) {
        total += entry.second.bytes;
    }
    LOG_INFO(HW_GPU, "Download trace, {} in 10 s: {} KiB over {} keys", title, total / 1024,
             sorted.size());
    for (size_t i = 0; i < (std::min<size_t>)(sorted.size(), 16); ++i) {
        const auto& [key, totals] = sorted[i];
        if (write_keys) {
            LOG_INFO(HW_GPU, "  write [{:#x} +{:#x}): {} KiB in {}", key >> 24, key & 0xffffff,
                     totals.bytes / 1024, totals.count);
        } else {
            LOG_INFO(HW_GPU, "  region {:#x}: {} KiB in {} copies", key << 16, totals.bytes / 1024,
                     totals.count);
        }
    }
}

inline void MaybeReport() {
    auto& state = Get();
    const auto now = std::chrono::steady_clock::now();
    if (now - state.last < std::chrono::seconds{10}) {
        return;
    }
    state.last = now;
    LOG_INFO(HW_GPU,
             "Download trace, CPU reads of GPU-written memory in 10 s: {} flushed ({} KiB), {} "
             "marked preflushable",
             state.reads_flushed, state.read_bytes_flushed / 1024, state.reads_marked);
    LogTop("flushed CPU reads by region", state.flushed_reads, false);
    LogTop("queued by GPU write", state.writes, true);
    LogTop("copied back by region", state.regions, false);
    state.writes.clear();
    state.regions.clear();
    state.flushed_reads.clear();
    state.reads_flushed = 0;
    state.reads_marked = 0;
    state.read_bytes_flushed = 0;
}

/// A GPU write over [address, address + size) queued `queued` bytes for the next fence.
inline void Queued(u64 address, u64 size, u64 queued) {
    auto& totals = Get().writes[(address << 24) | (std::min<u64>)(size, 0xffffff)];
    totals.bytes += queued;
    ++totals.count;
}

/// Buffer data copied back at a fence.
inline void Copied(u64 address, u64 size) {
    auto& totals = Get().regions[address >> 16];
    totals.bytes += size;
    ++totals.count;
    MaybeReport();
}

/// A guest CPU read of GPU-written [address, address + size): marked Preflushable, or flushed.
inline void Marked(u64 address, u64 size, bool preflush) {
    auto& state = Get();
    if (state.marks++ < 2000) {
        LOG_INFO(HW_GPU, "Download trace: CPU read of GPU-written [{:#x} +{:#x}) {}", address,
                 size, preflush ? "marked preflushable" : "flushed");
    }
    if (preflush) {
        ++state.reads_marked;
    } else {
        ++state.reads_flushed;
        state.read_bytes_flushed += size;
        auto& totals = state.flushed_reads[address >> 16];
        totals.bytes += size;
        ++totals.count;
    }
    // With fence downloads off (Switch 2 titles) Copied never runs: report from here too.
    MaybeReport();
}

} // namespace VideoCommon::Nx2DownloadTrace
