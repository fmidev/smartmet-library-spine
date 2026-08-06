// ======================================================================
/*!
 * \brief Reactor options
 */
// ======================================================================

#pragma once

#include "OTelOptions.h"
#include <macgyver/Optional.h>
#include <libconfig.h++>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace SmartMet
{
namespace Spine
{
// Pool specific options

struct PoolOptions
{
  // TODO: Fix naming scheme, now we set libconfig slowpool.maxthreads to be minsize
  //	std::size_t maxsize;
  unsigned int minsize = std::thread::hardware_concurrency();
  unsigned int maxrequeuesize = 100;
};

struct ThrottleOptions
{
  unsigned int start_limit = 50;    // start with max 50 active requests
  unsigned int restart_limit = 50;  // restart when down to 50 requests again
  unsigned int limit = 100;         // final max active requests
  unsigned int increase_rate = 10;  // increment current limit every 10 succesfull requests
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

  // The content codings this server offers, in preference order.
  //
  // Configured as a comma separated list ("zstd,gzip", "gzip"), which parse()
  // validates into contentCodings below. Only codings the server knows how to
  // produce, i.e. members of HTTP::supportedContentEncodings(), are accepted, so
  // the setting can narrow and reorder that list but not extend it. Use
  // compress=false to stop encoding altogether.
  //
  // The point of the setting is that a codec can be taken out of use across a
  // cluster by editing configuration, without waiting for a rebuild. Clients
  // that advertise a coding they cannot actually decode do exist, and the
  // symptom -- a client reporting our response as corrupt -- is indistinguishable
  // from a server side bug until someone can turn the codec off and see.
  std::string compresscodings;

  // Parsed form of compresscodings. Empty means HTTP::supportedContentEncodings(),
  // so that an Options object that has not been through parse() still offers the
  // built-in codings rather than silently none.
  std::vector<std::string> contentCodings;
  bool defaultlogging = true;
  bool lazylinking = true;
  bool stacktrace = false;

  // Reverse-DNS (PTR) resolution of the client IP for diagnostics / the admin
  // active-requests output. A slow or missing PTR record must never delay request
  // handling, so resolution is cached and performed entirely off the request
  // thread by background workers; the request thread only enqueues the IP and
  // output consumers read the cache (see Spine::HostInfo). Configured via the
  // "dns" config group (dns.resolve / dns.cachesize / dns.positivettl /
  // dns.negativettl / dns.threads); set dns.resolve=false to log the raw client
  // IP only and skip lookups.
  bool resolveClientHostName = true;              // master switch
  unsigned int clientHostNameCacheSize = 200000;  // max cached IP -> host name entries (LRU)
  unsigned int clientHostNamePositiveTtl = 3600;  // s a successful lookup stays cached
  unsigned int clientHostNameNegativeTtl = 60;    // s a failed lookup stays cached
  unsigned int clientHostNameThreads = 1;         // number of background resolver threads
  unsigned int clientHostNameMaxQueueSize = 1000;  // max pending (queued + in-flight) lookups

  ThrottleOptions throttle;

  unsigned int maxrequestsize = 131072;  // Limit incoming request sizes, 0 means unlimited

  // stale-while-revalidate: how long (seconds) a CDN/browser may serve a stale response while
  // fetching a fresh one in the background. 0 disables the directive.
  unsigned int staleWhileRevalidate = 60;

  // stale-if-error: how long (seconds) a CDN/browser may serve a stale response when the origin
  // returns an error (5xx, network failure, etc.). 0 disables the directive.
  unsigned int staleIfError = 86400;

  std::string accesslogdir{"/var/log/smartmet"};

  OTelOptions otel;

  PoolOptions adminpool;
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
