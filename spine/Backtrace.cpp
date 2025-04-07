#include "Backtrace.h"
#include <backtrace.h>

#include <iostream>
#include <sstream>
#include <cxxabi.h>
#include <fmt/format.h>

using SmartMet::Spine::Backtrace;

Backtrace Backtrace::instance;

thread_local Backtrace::Data Backtrace::data;

Backtrace::Backtrace()
{
    bt_state = backtrace_create_state(
        nullptr,     // No use to specify file name here
        1,           // Can be used from several threads
        &Backtrace::backtrace_error_callback,
        this         // Link to this instance
    );
}

Backtrace::~Backtrace() = default;

std::string Backtrace::make_backtrace()
{
    try
    {
        Backtrace::data.level = 0;
        Backtrace::data.out.str("");

        backtrace_full(
            reinterpret_cast<backtrace_state*>(instance.bt_state),
            1,  // Skip one frame (this method)
            &Backtrace::backtrace_full_callback,
            &Backtrace::backtrace_error_callback,
            nullptr);
    }
    catch (...)
    {
    }

    return Backtrace::data.out.str();
}

void Backtrace::backtrace_error_callback(void* data, const char* msg, int errnum)
{
    Backtrace::data.out << "Backtrace error: " << msg << " (" << errnum << ")";
    Backtrace::data.out << std::endl;
}

int Backtrace::backtrace_full_callback(
    void* data,
    std::uintptr_t pc,
    const char* filename,
    int lineno,
    const char* function)
{
    std::size_t level = ++Backtrace::data.level;
    std::ostringstream& oss = Backtrace::data.out;
    int status;
    std::size_t len = 1024;
    char* demangled = nullptr;
    try
    {
        demangled = new char[len];
        demangled = __cxxabiv1::__cxa_demangle(function, demangled, &len, &status);
        if (status != 0)
        {
            free(demangled);
            demangled = nullptr;
        }
    }
    catch (...)
    {
    }

    oss << "[" << fmt::format("{:04}", level) << "]:\n";
    if (filename && lineno > 0)
        oss << "    " << filename << ":" << lineno << '\n';
    oss << "    Address: " << fmt::format("{:x}", pc) << '\n';
    if (demangled)
        oss << "    Demangled symbol: " << demangled << '\n';
    else if (function)
        oss << "    Symbol: " << function << '\n';

    free(demangled);

    return 0;  // Continue backtracing
}
