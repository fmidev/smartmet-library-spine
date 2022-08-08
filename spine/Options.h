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
  unsigned int minsize = 10;
  unsigned int maxrequeuesize = 100;
};

struct ThrottleOptions
{
  unsigned int start_limit = 50;    // start with max 50 active requests
  unsigned int restart_limit = 50;  // restart when down to 50 requests again
  unsigned int limit = 100;         // final max active requests
  unsigned int increase_rate = 10;  // increment current limit every 10 succesfull requests
  unsigned int alert_limit = 100;   // start alert script at this many requests
  std::string alert_script;         // system command to run when the limit is broken
};

// Storage for parsed options

struct Options
{
  int port = 8080;
  bool encryptionEnabled = false;
  std::string encryptionCertificateFile;
  std::string encryptionPrivateKeyFile;
  std::string encryptionPasswordFile;
  std::string encryptionPassword;
  unsigned int timeout = 60;
  std::string directory{"/usr/share/smartmet"};
  std::string configfile{"/etc/smartmet/smartmet.conf"};
  std::string username;
  std::string locale{"fi_FI.UTF-8"};
  std::string new_handler{"default"};
  bool verbose = false;
  bool quiet = false;
  bool debug = false;
  bool logrequests = false;
  bool compress = false;
  unsigned int compresslimit = 1000;
  unsigned int coredump_filter = 0x33;  // Linux default is 0x33
  bool defaultlogging = true;
  bool lazylinking = true;
  bool stacktrace = false;

  ThrottleOptions throttle;

  unsigned int maxrequestsize = 131072;  // Limit incoming request sizes, 0 means unlimited

  std::string accesslogdir{"/var/log/smartmet"};

  PoolOptions slowpool;
  PoolOptions fastpool;

  libconfig::Config itsConfig;

  bool parse(int argc, char* argv[]);
  bool parseOptions(int argc, char* argv[]);
  void parseConfig();
  void report() const;

};  // struct Options

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
