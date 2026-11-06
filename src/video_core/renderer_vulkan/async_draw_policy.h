// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>

#include "common/common_types.h"

namespace Vulkan {

/// Whether a draw whose graphics pipeline is still building may be skipped (asynchronous
/// shaders) instead of waiting for the build. discard_native lets native (Switch 2) draws follow
/// the same heuristic (EDEN_NX2_ASYNC_DISCARD=1, from zortos293/Lichii#224).
constexpr bool CanDiscardPendingDraw(bool native, bool depth_enabled, u32 index_count,
                                    u32 vertex_count, bool discard_native = false) {
    // Native draws can produce persistent resources, even with depth enabled: only on opt-in.
    if (native && !discard_native) {
        return false;
    }
    // Preserve the legacy async-shader heuristic.
    return depth_enabled || (index_count > 6 && vertex_count > 6);
}

constexpr bool DefersShaderEmission(bool native, size_t pending, size_t capacity) {
    return native && pending < capacity;
}

} // namespace Vulkan
