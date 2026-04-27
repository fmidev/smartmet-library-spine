#include "MallocStats.h"

#include <dlfcn.h>

#include <mutex>
#include <sstream>
#include <string>

namespace SmartMet
{
namespace Spine
{
namespace
{
// jemalloc's exported entry point. Documented at
// https://jemalloc.net/jemalloc.3.html#malloc_stats_print -- jemalloc
// renames its symbols to plain `malloc` / `malloc_stats_print` /
// etc. when configured as the default allocator (which is the case
// for any sane jemalloc-enabled smartmet build). The signature is
// stable across every jemalloc 5.x release.
using je_malloc_stats_print_t = void (*)(void (*)(void*, const char*),
                                         void* /*cbopaque*/,
                                         const char* /*opts*/);

// mimalloc's stats callback API, available since 1.7. Older
// mimalloc versions only have `mi_stats_print(out)` which writes to
// stderr or a FILE*; we don't fall back to that because routing a
// stderr-only print through the admin plugin response would require
// open_memstream juggling on every call.
using mi_stats_print_out_t = void (*)(void (*)(const char*, void* /*arg*/),
                                      void* /*arg*/);

// Aggregator written into by both jemalloc's and mimalloc's
// callbacks. Lives on the calling thread's stack; the callbacks
// run synchronously from inside the print function so no
// synchronisation is needed.
struct Aggregator
{
  std::ostringstream os;
};

void je_write_cb(void* opaque, const char* str)
{
  static_cast<Aggregator*>(opaque)->os << str;
}

void mi_write_cb(const char* str, void* opaque)
{
  static_cast<Aggregator*>(opaque)->os << str;
}

}  // namespace

std::string getMallocAllocator()
{
  static std::string cached;
  static std::once_flag of;
  std::call_once(of,
                 []()
                 {
                   if (dlsym(RTLD_DEFAULT, "malloc_stats_print"))
                     cached = "jemalloc";
                   else if (dlsym(RTLD_DEFAULT, "mi_stats_print_out"))
                     cached = "mimalloc";
                   else if (dlsym(RTLD_DEFAULT, "malloc_info"))
                     cached = "glibc";
                   else
                     cached = "unknown";
                 });
  return cached;
}

std::string getMallocStats(const std::string& opts)
{
  Aggregator agg;

  // jemalloc path. Callback signature is (void*, const char*) —
  // string passed in is a chunk of the formatted output, not
  // necessarily a complete line. opts default "J" requests JSON;
  // jemalloc honours every other documented option flag too.
  if (auto* je_print = reinterpret_cast<je_malloc_stats_print_t>(
          dlsym(RTLD_DEFAULT, "malloc_stats_print")))
  {
    const char* je_opts = opts.empty() ? "J" : opts.c_str();
    je_print(je_write_cb, &agg, je_opts);
    return agg.os.str();
  }

  // mimalloc path. The opts argument is ignored because mimalloc's
  // text output is fixed-format; the smtop-side parser handles both
  // jemalloc-JSON and mimalloc-text shapes.
  if (auto* mi_print = reinterpret_cast<mi_stats_print_out_t>(
          dlsym(RTLD_DEFAULT, "mi_stats_print_out")))
  {
    mi_print(mi_write_cb, &agg);
    return agg.os.str();
  }

  // No supported allocator. Return a JSON-shaped error so the
  // smtop client can parse the result uniformly.
  return std::string{"{\"error\": \"no malloc_stats_print() symbol "}
         + "found in this process; allocator detected as "
         + getMallocAllocator() + "\"}";
}

}  // namespace Spine
}  // namespace SmartMet
