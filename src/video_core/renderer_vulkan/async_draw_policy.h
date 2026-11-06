// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>

#include "common/common_types.h"

namespace Vulkan {

/// Whether a draw whose graphics pipeline is still building may be skipped (asynchronous
/// shaders) instead of waiting for the build.
/// @param native         The draw comes from a Switch 2 (native Turing macro) program.
/// @param depth_enabled  The depth test is on for the draw.
/// @param index_count    Index count of the draw.
/// @param vertex_count   Vertex count of the draw.
/// @param discard_native Let native draws be skipped too (EDEN_NX2_ASYNC_DISCARD=1), here only
///                       when depth-tested.
/// @return True when the draw may be skipped.
constexpr bool CanDiscardPendingDraw(bool native, bool depth_enabled, u32 index_count,
                                    u32 vertex_count, bool discard_native = false) {
    // Native draws can produce persistent resources, even with depth enabled. They are only
    // skipped when the user opts in (EDEN_NX2_ASYNC_DISCARD=1), trading a possible missing
    // effect for no stall while a new pipeline builds.
    if (native && !discard_native) {
        return false;
    }
    // Opted-in native draws: only depth-tested ones (the 3D scene, redrawn every frame). With the
    // element-count rule Ocarina's minimap stayed blank for minutes: it is drawn once into a
    // texture without depth, and the skipped draw was never repeated until the next room.
    if (native) {
        return depth_enabled;
    }
    // Preserve the legacy async-shader heuristic.
    return depth_enabled || (index_count > 6 && vertex_count > 6);
}

/// Whether a native pipeline's SPIR-V emission is deferred to a worker: only while fewer than
/// `capacity` emissions are pending.
constexpr bool DefersShaderEmission(bool native, size_t pending, size_t capacity) {
    return native && pending < capacity;
}

} // namespace Vulkan
