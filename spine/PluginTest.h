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
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <dtl/dtl.hpp>
#include <macgyver/WorkQueue.h>
#include <algorithm>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <list>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace ba = boost::algorithm;
namespace fs = boost::filesystem;

namespace SmartMet
{
namespace Spine
{
namespace PluginTest
{
typedef boost::function<void(SmartMet::Spine::Reactor& reactor)> PreludeFunction;
int test(SmartMet::Spine::Options& options,
         PreludeFunction prelude,
         bool processresult = false,
         int num_threads = 1);

}  // namespace PluginTest
}  // namespace Spine
}  // namespace SmartMet

// ----------------------------------------------------------------------
// IMPLEMENTATION DETAILS
// ----------------------------------------------------------------------

namespace
{
// These are for debugging

void printRequest(SmartMet::Spine::HTTP::Request& request)
{
  try
  {
    std::cout << request.getMethodString() << ' ' << request.getURI() << ' '
              << request.getClientIP() << std::endl;

    auto params = request.getParameterMap();

    for (const auto& value : params)
    {
      std::cout << value.first << "=" << value.second << std::endl;
    }

    auto headers = request.getHeaders();

    for (const auto& value : headers)
    {
      std::cout << value.first << ":" << value.second << std::endl;
    }

    std::cout << request.getContent() << std::endl;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
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
      return "Diff size " + std::to_string(ret.size()) + " is too big (>5000)";
    return ret;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void add_to_path_list(fs::directory_entry& entry,
                      std::list<fs::path>* dest,
                      const std::list<fs::path>* top_parts,
                      unsigned max_files)
{
  try
  {
    std::vector<fs::path> curr_parts;
    fs::path item = entry.path();
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
          throw SmartMet::Spine::Exception(BCP, "Too many files!")
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
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::list<fs::path> recursive_directory_contents(const fs::path& top, unsigned max_files = 1000)
{
  try
  {
    using boost::bind;

    if (!boost::filesystem::exists(top) || !boost::filesystem::is_directory(top))
      throw SmartMet::Spine::Exception(
          BCP, "Could not open directory '" + top.string() + "' for reading!");

    std::list<fs::path> result;
    std::list<fs::path> top_parts;
    std::copy(top.begin(), top.end(), std::back_inserter(top_parts));
    std::for_each(fs::recursive_directory_iterator(top),
                  fs::recursive_directory_iterator(),
                  bind(&add_to_path_list, ::_1, &result, &top_parts, max_files));
    result.sort();
    return result;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------

std::string get_file_contents(const boost::filesystem::path& filename)
{
  try
  {
    std::string content;
    std::ifstream in(filename.c_str());
    if (in)
      content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return content;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------

void put_file_contents(const boost::filesystem::path& filename, const std::string& data)
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
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------

bool get_processed_response(const SmartMet::Spine::HTTP::Response& response,
                            const boost::filesystem::path& scriptfile,
                            std::string& result)
{
  try
  {
    using boost::filesystem::path;

    bool ok = false;
    FILE* handle = nullptr;
    path temp_fn = path("tmp") / scriptfile.filename();

    try
    {
      put_file_contents(temp_fn, result);
      std::string cmd = scriptfile.string() + " " + temp_fn.string();

      handle = popen(cmd.c_str(), "r");

      if (!handle)
        throw SmartMet::Spine::Exception(BCP, "The 'popen()' function call failed!");

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
    // boost::filesystem::remove(temp_fn);

    return ok;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------

bool process_query(const fs::path& fn, SmartMet::Spine::Reactor& reactor, bool processresult)
{
  using boost::filesystem::path;

  path inputfile("input");
  inputfile /= fn;

  const std::size_t padding = 90;
  const int dots = static_cast<int>(padding - inputfile.native().size());

  std::ostringstream out;

  out << fn.native() << ' ' << std::setw(dots) << std::setfill('.') << ". ";

  std::string input = get_file_contents(inputfile);

  // emacs keeps messing up the newlines, easier to make sure
  // the ending is correct this way, but do NOT touch POST queries

  if (boost::algorithm::ends_with(inputfile.string(), ".get"))
  {
    boost::algorithm::trim(input);
    input += "\r\n\r\n";
  }

  auto query = SmartMet::Spine::HTTP::parseRequest(input);

  bool ok = true;

  if (query.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    try
    {
      SmartMet::Spine::HTTP::Response response;

      auto view = reactor.getHandlerView(*query.second);
      if (!view)
      {
        ok = false;
        out << "FAILED TO HANDLE REQUEST STRING";
      }
      else
      {
        view->handle(reactor, *query.second, response);

        std::string result = get_full_response(response);

        if (processresult)
        {
          // Run script/executable to process the result (e.g. convert binary to ascii) for
          // validation
          //
          path scriptfile = path("scripts") / fn;

          if (exists(scriptfile))
          {
            ok = get_processed_response(response, scriptfile, result);

            if (!ok)
              out << "FAIL (result processing failed)";
          }
        }

        if (ok)
        {
          path outputfile = path("output") / fn;
          path failure_fn = path("failures") / fn;
          if (exists(outputfile) and is_regular_file(outputfile))
          {
            std::string output = get_file_contents(outputfile);

            if (result == output)
              out << "OK";
            else
            {
              // printMessage(&response);
              ok = false;
              out << "FAIL";
              boost::filesystem::create_directories(failure_fn.parent_path());
              put_file_contents(failure_fn, result);
              out << get_diff(outputfile.string(), failure_fn.string());
            }
          }
          else
          {
            ok = false;
            boost::filesystem::create_directories(failure_fn.parent_path());
            put_file_contents(failure_fn, result);
            out << "FAIL (expected result file '" << outputfile.string() << "' missing)";
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
    std::cout << out.str() + "\nFAILED TO PARSE REQUEST STRING\n" << std::flush;
  }
  else
  {
    ok = false;
    std::cout << out.str() + "PARSED REQUEST ONLY PARTIALLY\n" << std::flush;
  }
  return ok;
}

}  // namespace

namespace SmartMet
{
namespace Spine
{
namespace PluginTest
{
int test(SmartMet::Spine::Options& options,
         PreludeFunction prelude,
         bool processresult,
         int num_threads)
{
  using boost::filesystem::path;

  try
  {
    std::atomic<int> num_failed{0};
    options.parseConfig();
    SmartMet::Spine::Reactor reactor(options);
    prelude(reactor);

    std::list<path> inputfiles = recursive_directory_contents("input");

    // Run tests in parallel

    const auto executor = [&num_failed, &reactor, &processresult](const path& fn) {
      try
      {
        bool ok = process_query(fn, reactor, processresult);
        if (not ok)
          ++num_failed;
      }
      catch (...)
      {
        ++num_failed;
        SmartMet::Spine::Exception ex(BCP, "Test failed");
        ex.printError();
      }
    };

    Fmi::WorkQueue<path> workqueue(executor, num_threads);

    for (const path& fn : inputfiles)
      workqueue(fn);

    workqueue.join_all();

    if (num_failed > 0)
    {
      std::cout << "\n";
      std::cout << "*** " << num_failed << " test" << (num_failed == 1 ? "" : "s") << " of "
                << inputfiles.size() << " failed\n";
      std::cout << std::endl;
    }

    return num_failed > 0 ? 1 : 0;
  }
  catch (...)
  {
    SmartMet::Spine::Exception e(BCP, "Plugin test failed!", nullptr);
    std::cout << e.getStackTrace() << std::endl;
    return 1;
  }
}

}  // namespace PluginTest
}  // namespace Spine
}  // namespace SmartMet
