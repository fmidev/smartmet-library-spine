// ======================================================================
/*!
 * \brief Standard way of testing plugins with text output
 *
 * The code is header only since we do not want to increase the
 * size of the dynamic library with test code.
 *
 * Note: If the input file has string 'Fail' in it, stderr
 *       will be redirected to /dev/null.
 */
// ======================================================================

#pragma once

#include "HTTP.h"
#include "Options.h"
#include "Reactor.h"
#include <boost/algorithm/string.hpp>
#include <boost/bind/bind.hpp>
#include <boost/range/iterator_range.hpp>
#include <fmt/format.h>
#include <dtl/dtl.hpp>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/FileSystem.h>
#include <macgyver/StringConversion.h>
#include <macgyver/WorkQueue.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <list>
#include <numeric>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

// Needed only for WGS84 branch tests
#include <newbase/NFmiGlobals.h>

namespace ba = boost::algorithm;
namespace fs = std::filesystem;

namespace SmartMet
{
namespace Spine
{
using PreludeFunction = boost::function<void(SmartMet::Spine::Reactor& reactor)>;

}  // namespace Spine
}  // namespace SmartMet

// ----------------------------------------------------------------------
// IMPLEMENTATION DETAILS
// ----------------------------------------------------------------------

// These are for debugging

void printRequest(const SmartMet::Spine::HTTP::Request& request)
{
  try
  {
    std::cout << request.getMethodString() << ' ' << request.getURI() << ' '
              << request.getClientIP() << std::endl;

    auto params = request.getParameterMap();

    for (const auto& value : params)
      std::cout << value.first << "=" << value.second << std::endl;

    auto headers = request.getHeaders();

    for (const auto& value : headers)
      std::cout << value.first << ":" << value.second << std::endl;

    std::cout << request.getContent() << std::endl;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ---------------------------------------------------------------------

std::vector<std::string> read_file(const std::string& fn)
{
  try
  {
    std::ifstream input(fn.c_str());
    std::vector<std::string> result;
    std::string line;
    while (std::getline(input, line))
    {
      result.push_back(ba::trim_right_copy_if(line, ba::is_any_of("\r\n")));
    }
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string get_diff(const std::string& src, const std::string& dest)
{
  try
  {
    const auto f1 = read_file(src);
    const auto f2 = read_file(dest);
    dtl::Diff<std::string> d(f1, f2);
    d.compose();
    d.composeUnifiedHunks();

    std::ostringstream out;
    d.printUnifiedFormat(out);
    std::string ret = out.str();

    if (ret.size() > 5000)
      return "  Diff size " + std::to_string(ret.size()) + " is too big (>5000)";
    return "\n" + ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------

bool check_path(bool ok, const fs::path& p)
{
  try
  {
    const std::string& name = p.string();
    ok &= not ba::starts_with(name, ".") and not ba::starts_with(name, "#") and
          not ba::ends_with(name, "~");
    return ok;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void add_to_path_list(const fs::directory_entry& entry,
                      std::list<fs::path>* dest,
                      const std::list<fs::path>* top_parts,
                      unsigned max_files)
{
  try
  {
    std::vector<fs::path> curr_parts;
    const auto& item = entry.path();
    std::copy(item.begin(), item.end(), std::back_inserter(curr_parts));
    if ((top_parts->size() < curr_parts.size()) && fs::is_regular_file(item))
    {
      auto it1 = curr_parts.begin();
      it1 += top_parts->size();
      if (std::equal(curr_parts.begin(), it1, top_parts->begin()))
      {
        fs::path result;
        for (auto it = it1; it != curr_parts.cend(); ++it)
        {
          result /= *it;
        }

        if (dest->size() >= max_files)
        {
          throw Fmi::Exception(BCP, "Too many files!")
              .addParameter("Files", std::to_string(dest->size()))
              .addParameter("Max files", std::to_string(max_files));
        }

        if (std::accumulate(it1, curr_parts.end(), true, &check_path))
        {
          dest->push_back(result);
        }
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::list<fs::path> recursive_directory_contents(const fs::path& top, unsigned max_files = 1000)
{
  try
  {
    using boost::bind;

    if (!std::filesystem::exists(top) || !std::filesystem::is_directory(top))
      throw Fmi::Exception(BCP, "Could not open directory '" + top.string() + "' for reading!");

    std::list<fs::path> result;
    std::list<fs::path> top_parts;
    std::copy(top.begin(), top.end(), std::back_inserter(top_parts));
    std::for_each(fs::recursive_directory_iterator(top),
                  fs::recursive_directory_iterator(),
                  bind(&add_to_path_list, boost::placeholders::_1, &result, &top_parts, max_files));
    result.sort();
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------

std::string get_file_contents(const std::filesystem::path& filename)
{
  try
  {
    std::string content;
    std::ifstream raw(filename.c_str());
    Fmi::IStream in(raw, filename.c_str());
    if (in)
      content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

    return content;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------

void put_file_contents(const std::filesystem::path& filename, const std::string& data)
{
  try
  {
    std::ofstream out(filename.c_str());
    if (out)
    {
      out << data;
      out.close();
    }
    else
      std::cerr << "Failed to open " << filename << " for writing" << std::endl;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------

std::string get_full_response(SmartMet::Spine::HTTP::Response& response)
{
  try
  {
    std::string result = response.getContent();

    if (result.empty())
      return result;

    if (response.hasStreamContent())
    {
      while (true)
      {
        std::string tmp = response.getContent();
        if (tmp.empty())
          break;
        result += tmp;
      }
    }

    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------

bool get_processed_response(const std::filesystem::path& scriptfile, std::string& result)
{
  try
  {
    using std::filesystem::path;

    bool ok = false;
    FILE* handle = nullptr;
    path temp_fn = path("tmp") / scriptfile.filename();

    try
    {
      put_file_contents(temp_fn, result);
      std::string cmd = scriptfile.string() + " " + temp_fn.string();

      handle = popen(cmd.c_str(), "r");  // NOLINT this is safe enough in tests

      if (!handle)
        throw Fmi::Exception(BCP, "The 'popen()' function call failed!");

      const int buflen = 1024;
      char buf[buflen];
      size_t nbytes;

      std::ostringstream os;

      while ((nbytes = fread(buf, 1, sizeof(buf), handle)) > 0)
        os << std::string(buf, nbytes);

      result = os.str();

      ok = true;
    }
    catch (std::exception& e)
    {
      std::cout << "EXCEPTION: " << e.what() << std::endl;
    }
    catch (...)
    {
      std::cout << "UNKNOWN EXCEPTION" << std::endl;
    }

    if (handle)
      pclose(handle);

    // Keeping the failures helps debugging, do not delete:
    // std::filesystem::remove(temp_fn);

    return ok;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

namespace SmartMet
{
namespace Spine
{
class PluginTest
{
 public:
  // Run tests with give server options and header printer
  int run(SmartMet::Spine::Options& options, const PreludeFunction& prelude) const;

  // Deprecated function, switched from using a namespace to a class
  static int test(SmartMet::Spine::Options& options, PreludeFunction prelude, int num_threads = 1);

  // Run tests in parallel?
  void setNumberOfThreads(int num_threads) { mNumThreads = num_threads; }

  // Input, expected output, and errorneous output directories
  void setInputDir(const std::string& dir) { mInputDir = dir; }
  void setOutputDir(const std::string& dir) { mOutputDir = dir; }
  void setFailDir(const std::string& dir) { mFailDir = dir; }
  void addIgnoreList(const std::string& fileName) { ignore_lists.push_back(fileName); }
  void setFilter(const std::string& filter) { this->filter.reset(new std::regex(filter)); }

 private:
  struct IgnoreInfo
  {
    std::string description;
    bool found = false;

    explicit IgnoreInfo(std::string description = "") : description(std::move(description)) {}
  };

  using IgnoreMap = std::map<std::string, IgnoreInfo>;

  int mNumThreads = 1;

  std::string mInputDir{"input"};
  std::string mOutputDir{"output"};
  std::string mFailDir{"failures"};
  std::vector<std::string> ignore_lists;

  bool process_query(const fs::path& fn,
                     std::size_t padding,
                     SmartMet::Spine::Reactor& reactor,
                     IgnoreMap& ignores) const;

  std::unique_ptr<std::regex> filter;
  std::vector<std::string> read_ignore_list(const std::string& dir) const;
  static std::vector<std::string> read_ignore_file(const std::string& fn);
};  // class PluginTest

// Deprecated
int PluginTest::test(SmartMet::Spine::Options& options, PreludeFunction prelude, int num_threads)
{
  PluginTest tests;
  tests.setNumberOfThreads(num_threads);
  return tests.run(options, prelude);
}

int PluginTest::run(SmartMet::Spine::Options& options, const PreludeFunction& prelude) const
{
  using std::filesystem::path;

  try
  {
    // Use any available port
    options.port = 0;

    std::atomic<int> num_failed{0};
    options.parseConfig();
    SmartMet::Spine::Reactor reactor(options);
    reactor.init();
    if (Reactor::isShuttingDown())
      throw Fmi::Exception(BCP, "Reactor shutdown detected while init phase");

    prelude(reactor);

    const auto inputfiles = recursive_directory_contents(mInputDir);

    // Establish required padding
    std::size_t padding = 20;
    for (const auto& name : inputfiles)
      padding = std::max(padding, name.native().size() + mInputDir.size() + 1);
    padding += 5;

    IgnoreMap ignores;
    for (const auto& ignore : read_ignore_list(mInputDir))
    {
      std::size_t pos = ignore.find_first_of(" \t");
      if (pos == std::string::npos)
      {
        ignores.emplace(ignore, IgnoreInfo());
      }
      else
      {
        ignores.emplace(ignore.substr(0, pos),
                        IgnoreInfo(ba::trim_copy_if(ignore.substr(pos + 1), ba::is_any_of(" \t"))));
      }
    }

    // Run tests in parallel

    const auto executor = [this, &padding, &num_failed, &reactor, &ignores](const path& fn)
    {
      try
      {
        bool ok = process_query(fn, padding, reactor, ignores);
        if (not ok)
          ++num_failed;
      }
      catch (...)
      {
        ++num_failed;
        Fmi::Exception ex(BCP, "Test failed");
        ex.printError();
      }
    };

    Fmi::WorkQueue<path> workqueue(executor, mNumThreads);

    for (const path& fn : inputfiles)
      workqueue(fn);

    workqueue.join_all();

    for (const auto& item : ignores)
    {
      if (not item.second.found)
      {
        std::cout << "WARNING: test '" << item.first << "' specified in ignores is not found"
                  << std::endl;
      }
    }

    if (num_failed > 0)
    {
      std::cout << "\n";
      std::cout << "*** " << num_failed << " test" << (num_failed == 1 ? "" : "s") << " of "
                << inputfiles.size() << " failed\n";
      std::cout << std::endl;
    }

    reactor.shutdown();

    return num_failed > 0 ? 1 : 0;
  }
  catch (...)
  {
    Fmi::Exception e(BCP, "Plugin test failed!", nullptr);
    std::cout << e.getStackTrace() << std::endl;
    return 1;
  }
}

// ----------------------------------------------------------------------

bool PluginTest::process_query(const fs::path& fn,
                               std::size_t padding,
                               SmartMet::Spine::Reactor& reactor,
                               IgnoreMap& ignores) const
{
  using std::filesystem::path;

  const bool is_tty = isatty(fileno(stdout));
  const std::string fg_fn = is_tty ? ANSI_FG_CYAN : "";
  const std::string fg_red = is_tty ? ANSI_FG_RED : "";
  const std::string fg_green = is_tty ? ANSI_FG_GREEN : "";
  const std::string fg_default = is_tty ? ANSI_FG_DEFAULT : "";

  path inputfile(mInputDir);
  inputfile /= fn;

  const int dots = static_cast<int>(padding - inputfile.native().size());

  std::ostringstream out;

  out << fg_fn << fn.native() << fg_default << ' ' << std::setw(dots) << std::setfill('.') << ". ";

  std::string input = get_file_contents(inputfile);

  // emacs keeps messing up the newlines, easier to make sure
  // the ending is correct this way, but do NOT touch POST queries

  if (boost::algorithm::ends_with(inputfile.string(), ".get") or
      boost::algorithm::ends_with(inputfile.string(), ".options"))
  {
    Fmi::trim(input);
    input += "\r\n\r\n";
  }

  auto query = SmartMet::Spine::HTTP::parseRequest(input);

  bool ok = true;

  if (query.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    try
    {
      SmartMet::Spine::HTTP::Response response;

      const auto start_time = std::chrono::system_clock::now();

      auto view = reactor.getHandlerView(*query.second);
      if (!view)
      {
        ok = false;
        out << fg_red << "FAILED TO HANDLE REQUEST STRING" << fg_default;
      }
      else
      {
        auto ignores_it = ignores.find(fn.string());
        bool filter_ok = not filter or std::regex_search(fn.string(), *filter);
        if (ignores_it != ignores.end())
        {
          ignores_it->second.found = true;
          ok = true;
          out << "IGNORED IN THIS SETUP";
          if (!ignores_it->second.description.empty())
          {
            out << ": " << ignores_it->second.description;
          }
        }
        else if (not filter_ok)
        {
          ok = true;
          out << "IGNORED BY FILTER";
        }
        else
        {
          view->handle(reactor, *query.second, response);

          const auto done_time = std::chrono::system_clock::now();
          const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(done_time - start_time)
                                    .count();
          const std::string time_str = fmt::format("{:6.3f}", 0.001*duration);

          std::string result;

          switch (query.second->getMethod())
          {
            default:
              result = get_full_response(response);
              break;

            case HTTP::RequestMethod::OPTIONS:
              response.removeHeader("Date");
              result = response.toString();
              break;
          }

          // Run script/executable to process the result (e.g. convert binary to ascii) for
          // validation
          //
          path scriptfile = path("scripts") / fn;

          if (exists(scriptfile))
          {
            ok = get_processed_response(scriptfile, result);

            if (!ok)
              out << fg_red << "FAIL" << fg_default << "  " << time_str << "s "
                  << fg_red << "(result processing failed)" << fg_default;
          }

          if (ok)
          {
            path outputfile = path(mOutputDir) / fn;
            path failure_fn = path(mFailDir) / fn;

            if (!Fmi::LegacyMode())
            {
              path wgs84outputfile = path(mOutputDir) / (fn.native() + ".wgs84");
              if (exists(wgs84outputfile))
              {
                outputfile = wgs84outputfile;
                failure_fn = path(mFailDir) / (fn.native() + ".wgs84");
              }
            }

            const auto real_output_file = Fmi::lookup_file(outputfile.string());
            if (real_output_file)
            {
              std::string output = get_file_contents(*real_output_file);

              if (result == output)
                out << fg_green << "OK  " << fg_default << "  " << time_str << "s";
              else
              {
                ok = false;
                out << fg_red << "FAIL" << fg_default << "  " << time_str << "s";
                std::filesystem::create_directories(failure_fn.parent_path());
                put_file_contents(failure_fn, result);
                out << get_diff(outputfile.string(), failure_fn.string());
              }
            }
            else
            {
              ok = false;
              std::filesystem::create_directories(failure_fn.parent_path());
              put_file_contents(failure_fn, result);
              out << fg_red << "FAIL" << fg_default << "  " << time_str
                  << "  (expected result file '" << fg_red << outputfile.string()
                  << fg_default << "' missing)";
            }
          }
        }
      }
      std::cout << out.str() + "\n" << std::flush;
    }
    catch (std::exception& e)
    {
      ok = false;
      std::cout << out.str() + "\nEXCEPTION: " + e.what() + "\n" << std::flush;
    }
    catch (...)
    {
      ok = false;
      std::cout << out.str() + "\nUNKNOWN EXCEPTION\n" << std::flush;
    }
  }
  else if (query.first == SmartMet::Spine::HTTP::ParsingStatus::FAILED)
  {
    ok = false;
    std::cout << out.str() + fg_red + "\nFAILED TO PARSE REQUEST STRING\n" + fg_default << std::flush;
  }
  else
  {
    ok = false;
    std::cout << out.str() + fg_red + "PARSED REQUEST ONLY PARTIALLY\n" + fg_default << std::flush;
  }
  return ok;
}

std::vector<std::string> PluginTest::read_ignore_list(const std::string& dir) const
{
  auto files = ignore_lists;
  if (files.empty())
  {
    files.push_back(dir + "/" + ".testignore");
  }

  std::vector<std::string> result;
  for (const auto& fn : files)
  {
    std::vector<std::string> a2 = read_ignore_file(fn);
    std::copy(a2.begin(), a2.end(), std::back_inserter(result));
  }
  std::sort(result.begin(), result.end());
  return result;
}

std::vector<std::string> PluginTest::read_ignore_file(const std::string& fn)
{
  try
  {
    std::ifstream input(fn.c_str());
    std::vector<std::string> result;
    std::string line;
    bool input_ok = bool(input);
    if (input_ok)
    {
      std::cout << "### Reading test ignores from " << fn << std::endl;
    }
    while (std::getline(input, line))
    {
      ba::trim_if(line, ba::is_any_of(" \t\r\n"));
      std::string prefix = "      ";
      if (line.length() > 0)
      {
        if (line[0] != ';' and line[0] != '#')
        {
          result.push_back(line);
          prefix = "IGNORE";
        }
      }
      std::cout << prefix << " | " << line << std::endl;
    }
    if (input_ok)
    {
      std::cout << "### " << result.size() << " ignores read from " << fn << std::endl << std::endl;
    }
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
