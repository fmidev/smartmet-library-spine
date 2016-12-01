// ======================================================================
/*!
 * \brief Reactor options
 */
// ======================================================================

#pragma once

#include "ConfigBase.h"
#include <boost/shared_ptr.hpp>
#include <string>

namespace SmartMet
{
namespace Spine
{
// Pool specific options

struct PoolOptions
{
  // TODO: Fix naming scheme, now we set libconfig slowpool.maxthreads to be minsize
  //	std::size_t maxsize;
  std::size_t minsize;
  std::size_t maxrequeuesize;

  PoolOptions();
};

// Storage for parsed options

struct Options
{
  int port;
  long timeout;
  std::string directory;
  std::string configfile;
  std::string username;
  std::string locale;
  bool verbose;
  bool quiet;
  bool debug;
  bool logrequests;
  bool compress;
  std::size_t compresslimit;
  bool defaultlogging;
  bool lazylinking;

  std::string accesslogdir;

  PoolOptions slowpool;
  PoolOptions fastpool;

  boost::shared_ptr<ConfigBase> itsConfig;

  Options();
  bool parse(int argc, char* argv[]);
  bool parseOptions(int argc, char* argv[]);
  void parseConfig();
  void report() const;

};  // struct Options

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
