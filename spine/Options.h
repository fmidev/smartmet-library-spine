// ======================================================================
/*!
 * \brief Reactor options
 */
// ======================================================================

#pragma once

#include <boost/shared_ptr.hpp>
#include <libconfig.h++>
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
  unsigned int minsize;
  unsigned int maxrequeuesize;
  PoolOptions();
};

// Storage for parsed options

struct Options
{
  int port;
  unsigned int timeout;
  std::string directory;
  std::string configfile;
  std::string username;
  std::string locale;
  bool verbose;
  bool quiet;
  bool debug;
  bool logrequests;
  bool compress;
  unsigned int compresslimit;
  bool defaultlogging;
  bool lazylinking;

  unsigned int maxactiverequests;  // 0 means unlimited
  unsigned int maxrequestsize;     // Limit incoming request sizes, 0 means unlimited

  std::string accesslogdir;

  PoolOptions slowpool;
  PoolOptions fastpool;

  libconfig::Config itsConfig;

  Options();
  bool parse(int argc, char* argv[]);
  bool parseOptions(int argc, char* argv[]);
  void parseConfig();
  void report() const;

};  // struct Options

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
