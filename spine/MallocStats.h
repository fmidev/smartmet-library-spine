// ======================================================================
/*!
 * \brief Allocator statistics retrieval via dynamic-symbol probing.
 *
 * Resolves the running process's allocator at runtime by looking for
 * jemalloc's `malloc_stats_print` or mimalloc's `mi_stats_print_out`
 * symbol (both auto-overrided when LD_PRELOADed or linked). Returns
 * the allocator's machine-readable stats dump as a string — JSON for
 * jemalloc, mimalloc's text format for mimalloc.
 *
 * The smartmet-monitor `Heap` panel polls this via
 * `?what=mallocstats` to render fragmentation and per-arena usage
 * without requiring server-side instrumentation hooks beyond the one
 * dlsym call.
 */
// ======================================================================

#pragma once

#include <string>

namespace SmartMet
{
namespace Spine
{
/// Detected allocator for this process: "jemalloc", "mimalloc",
/// "glibc", or "unknown". Result is cached after the first call.
std::string getMallocAllocator();

/// Stats dump from the active allocator. For jemalloc, `opts` is
/// passed through verbatim to `malloc_stats_print` and defaults to
/// "J" (JSON output); see jemalloc's documentation for the full
/// option set ("g" = general, "m" = merged, "d" = detailed,
/// "a" = arenas, "b" = bins, "l" = large allocations, "x" = mutex
/// stats). For mimalloc the `opts` argument is ignored.
///
/// Returns a string — empty when no supported allocator was
/// detected. Never throws.
std::string getMallocStats(const std::string& opts = "J");

}  // namespace Spine
}  // namespace SmartMet
