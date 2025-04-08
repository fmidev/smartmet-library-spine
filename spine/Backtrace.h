#pragma once

#include <cstdint>
#include <string>
#include <sstream>

namespace SmartMet
{
namespace Spine
{

// ======================================================================
class Backtrace final
{
    Backtrace();
public:
    ~Backtrace();
    Backtrace(const Backtrace&) = delete;
    Backtrace& operator=(const Backtrace&) = delete;
    Backtrace(Backtrace&&) = delete;
    Backtrace& operator=(Backtrace&&) = delete;

    static std::string make_backtrace() noexcept;
private:
    static Backtrace instance;

    static void backtrace_error_callback(void* data, const char* msg, int errnum) noexcept;

    static int backtrace_full_callback(
        void* data,
        std::uintptr_t pc,
        const char* filename,
        int lineno,
        const char* function) noexcept;

    void *bt_state;

    struct Data
    {
        std::size_t level;
        std::ostringstream out;
    };

    static thread_local Data data;
};

std::string backtrace();

}  // namespace Spine
}  // namespace SmartMet
