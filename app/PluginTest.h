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

#include "Options.h"
#include "Reactor.h"
#include <filesystem>
#include <functional>
#include <regex>

namespace SmartMet
{
namespace Spine
{

using PreludeFunction = std::function<void(SmartMet::Spine::Reactor& reactor)>;

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

  bool process_query(const std::filesystem::path& fn,
                     std::size_t padding,
                     SmartMet::Spine::Reactor& reactor,
                     IgnoreMap& ignores) const;

  std::unique_ptr<std::regex> filter;
  std::vector<std::string> read_ignore_list(const std::string& dir) const;
  static std::vector<std::string> read_ignore_file(const std::string& fn);
};  // class PluginTest

}  // namespace Spine
}  // namespace SmartMet
