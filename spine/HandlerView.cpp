#include "HandlerView.h"
#include "Convenience.h"
#include "FmiApiKey.h"
#include "Reactor.h"
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <macgyver/DateTime.h>
#include <macgyver/Exception.h>
#include <macgyver/Join.h>
#include <macgyver/StringConversion.h>

using namespace std::literals;

namespace
{
bool isNotOld(const Fmi::DateTime& target, const SmartMet::Spine::LoggedRequest& compare)
{
  return compare.getRequestEndTime() > target;
}

// Returns the iterator of the first request that has not yet been
// flushed to disk. The marker convention is: ``end()`` means "no
// entry has been flushed yet" (std::list iterators have no
// "before-begin" position so we use end() as the sentinel).
SmartMet::Spine::LogListType::iterator firstUnflushed(
    SmartMet::Spine::LogListType& log,
    SmartMet::Spine::LogListType::iterator marker)
{
  if (marker == log.end())
    return log.begin();
  return std::next(marker);
}

}  // namespace

namespace p = std::placeholders;

namespace SmartMet
{
namespace Spine
{
HandlerView::HandlerView(ContentHandler theHandler,
                         std::shared_ptr<IPFilter::IPFilter> theIpFilter,
                         SmartMetPlugin* thePlugin,
                         const std::string& theResource,
                         bool loggingStatus,
                         bool isprivate,
                         const std::set<std::string>& supportedPostContexts,
                         const std::string& accessLogDir,
                         const OTelOptions& otelOptions)
    : itsHandler(std::move(theHandler)),
      itsIpFilter(std::move(theIpFilter)),
      itsPlugin(thePlugin),
      itsResource(theResource),
      itsPrivate(isprivate),
      isLogging(loggingStatus),
      itsLastFlushedRequest(itsRequestLog.begin()),
      itsAccessLog(new AccessLogger(theResource, accessLogDir)),
      itsOTelLog(otelOptions.enabled ? std::make_unique<OTelLogger>(theResource, otelOptions)
                                     : nullptr),
      checkPostContentType(true)
{
  try
  {
    if (isLogging)
      itsAccessLog->start();

    if (itsOTelLog)
      itsOTelLog->start();

    // Insert supported POST contexts
    itsSupportedPostContents.insert("application/x-www-form-urlencoded"s);
    for (const auto& content : supportedPostContexts)
      itsSupportedPostContents.insert(Fmi::ascii_tolower_copy(content));

    itsSupportedPostContentsString = Fmi::join(itsSupportedPostContents, ", ");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

HandlerView::HandlerView(
    ContentHandler theHandler,
    const std::string& accessLogDir,
    const std::optional<std::string>& name,
    const OTelOptions& otelOptions)

    : itsHandler(std::move(theHandler)),
      itsIsCatchNoMatch(true),
      isLogging(name.has_value()),
      itsLastFlushedRequest(itsRequestLog.begin()),
      itsOTelLog(otelOptions.enabled ? std::make_unique<OTelLogger>(
                                           name.value_or("default-handler"), otelOptions)
                                     : nullptr),
      checkPostContentType(false)
{
  if (name)
  {
    itsAccessLog = std::make_unique<AccessLogger>(*name, accessLogDir);
    itsAccessLog->start();
  }
  if (itsOTelLog)
    itsOTelLog->start();
}

HandlerView::~HandlerView()
{
  flushLog();
}

bool HandlerView::handle(Reactor& theReactor,
                         const HTTP::Request& theRequest,
                         HTTP::Response& theResponse)
{
  try
  {
    if (!itsIsCatchNoMatch)
    {
      if (itsIpFilter != nullptr)
      {
        if (!itsIpFilter->match(theRequest.getClientIP()))
        {
          // False means ip filter blocked this
          return false;
        }
      }
    }

    if ((!isLogging || !itsAccessLog) && !itsOTelLog)
    {
      // No logging of any kind — take the fast path
      auto key = theReactor.insertActiveRequest(theRequest);
      try
      {
        itsHandler(theReactor, theRequest, theResponse);
        theReactor.removeActiveRequest(key, theResponse.getStatus());
      }
      catch (...)
      {
        theReactor.removeActiveRequest(key, theResponse.getStatus());
        throw;
      }
    }
    else
    {
      const auto method = theRequest.getMethod();
      if (method == HTTP::RequestMethod::OPTIONS)
      {
        theResponse = HTTP::Response::stockOptionsResponse({"GET", "POST", "OPTIONS"});

        // Checking for CORS preflight headers
        // https://developer.mozilla.org/en-US/docs/Glossary/Preflight_request
        if (theRequest.getHeader("Access-Control-Request-Method"))
        {
          // Clone header 'Allow' to 'Access-Control-Allow-Methods' for CORS
          auto h1 = theResponse.getHeader("Allow");
          assert(bool(h1));  // HTTP::Response::stockOptionsResponse should have set this header
          theResponse.setHeader("Access-Control-Allow-Methods", *h1);

          auto opt_origin = theRequest.getHeader("Origin");
          if (opt_origin)
          {
            theResponse.setHeader("Access-Control-Allow-Origin", *opt_origin);
          }

          auto opt_req_headers = theRequest.getHeader("Access-Control-Request-Headers");
          if (opt_req_headers)
          {
            // FIXME: Should be use actually supported headers here.
            //        Let us copy requested headers to the response for now
            theResponse.setHeader("Access-Control-Allow-Headers", *opt_req_headers);
          }

          theResponse.setHeader("Access-Control-Max-Age", "86400");
        }
        return true;
      }

      if (method == HTTP::RequestMethod::POST)
      {
        // Check that content type is supported
        auto opt_content_type = theRequest.getHeader("Content-Type");
        if (!opt_content_type)
        {
          // No content type, reject the request
          theResponse.setStatus(HTTP::bad_request);
          theResponse.setContent("Content-Type header is required for POST requests.\n"
            "Allowed: " + itsSupportedPostContentsString);
          return true;
        }

        // Normalize content type (remove parameters)
        std::string content_type = Fmi::ascii_tolower_copy(*opt_content_type);
        const std::size_t sep = content_type.find(';');
        if (sep != std::string::npos)
          content_type = content_type.substr(0, sep);

        if (checkPostContentType
           && itsSupportedPostContents.find(content_type) == itsSupportedPostContents.end())
        {
          // Unsupported content type
          theResponse.setStatus(HTTP::not_implemented);
          theResponse.setContent("Unsupported Content-Type '" + content_type + "',\n"
            "Allowed: " + itsSupportedPostContentsString + "\n");
          return true;
        }
      }
      else if (method != HTTP::RequestMethod::GET)
      {
        // Unsupported method
        theResponse.setStatus(HTTP::not_implemented);
        theResponse.setHeader("Allow", "GET, POST, OPTIONS");
        return true;
      }

      auto key = theReactor.insertActiveRequest(theRequest);
      // CPU-time bracketing via CLOCK_THREAD_CPUTIME_ID. The clock
      // advances only while THIS thread is on-CPU, so the resulting
      // duration measures actual compute time (user + kernel) and
      // excludes off-CPU stalls (lock waits, I/O, sleeps) that the
      // wall-clock difference would include. ?what=servicestats
      // surfaces the average as AverageCPUMs alongside the existing
      // wall-clock AverageDuration so operators can read CPU-bound
      // vs wait-bound handlers at a glance. The clock_gettime call
      // itself resolves through the vDSO on modern kernels (~10 ns)
      // so the per-request overhead is unmeasurable next to the
      // existing wall-clock bracketing.
      struct timespec cpu_before
      {
      };
      struct timespec cpu_after
      {
      };
      ::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_before);
      auto before = Fmi::MicrosecClock::universal_time();

      std::exception_ptr error;
      try
      {
        itsHandler(theReactor, theRequest, theResponse);
      }
      catch (boost::thread_interrupted&)
      {
        // Let thread interruption exceptions pass through
        throw;
      }
      catch (...)
      {
        error = std::current_exception();
      }
      auto accessDuration = Fmi::MicrosecClock::universal_time() - before;
      ::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_after);
      theReactor.removeActiveRequest(key, theResponse.getStatus());

      // Convert the timespec delta to a Fmi::TimeDuration. Carry the
      // nanosecond field if it went negative across a second
      // boundary; split into seconds + microseconds to avoid the
      // 32-bit overflow of Fmi::Microseconds(int) on long-running
      // requests.
      long cpu_secs = cpu_after.tv_sec - cpu_before.tv_sec;
      long cpu_nsec = cpu_after.tv_nsec - cpu_before.tv_nsec;
      if (cpu_nsec < 0)
      {
        --cpu_secs;
        cpu_nsec += 1'000'000'000L;
      }
      const auto cpuDuration =
          Fmi::Seconds(static_cast<int>(cpu_secs)) +
          Fmi::Microseconds(static_cast<int>(cpu_nsec / 1000));

      WriteLock lock(itsLoggingMutex);

      auto etag   = theResponse.getHeader("ETag");
      auto apikey = FmiApiKey::getFmiApiKey(theRequest);

      // Build the LoggedRequest used by both file access log and OTel
      const LoggedRequest logged(theRequest.getURI(),
                                 Fmi::MicrosecClock::local_time(),
                                 accessDuration,
                                 cpuDuration,
                                 theResponse.getStatusString(),
                                 theRequest.getClientIP(),
                                 theRequest.getMethodString(),
                                 theResponse.getVersion(),
                                 theResponse.getContentLength(),
                                 (etag ? *etag : "-"),
                                 (apikey ? *apikey : "-"));

      if (isLogging)  // Need to check this again because it may have changed in previous request
      {
        // Insert new request to the logging list
        itsRequestLog.push_back(logged);
      }

      // OTel export is independent of file logging toggle.
      // BatchSpanProcessor queues the span internally and returns immediately.
      if (itsOTelLog)
        itsOTelLog->log(logged);

      if (error)
        std::rethrow_exception(error);
    }

    return true;
  }
  catch (...)
  {
    std::cerr << Fmi::Exception::Trace(BCP, "Operation failed! HandlerView::handle aborted") << std::endl;
    return true;
  }
}

void HandlerView::setLogging(bool newStatus)
{
  try
  {
    // NoMatchHandler may be configured with both logging enabled and disabled.
    // Do nothing if access log pointer is empty. No need to lock mutex either
    // as logger pointer is not changed after construction.
    if (!itsAccessLog)
    {
      return;
    }

    // Snapshot of pending entries to flush, captured under the lock
    // and written to disk after we drop it. Same lock-vs-IO split
    // as cleanLog / flushLog: a high-traffic log queue can take
    // seconds to flush; holding the WriteLock for that long stalls
    // every concurrent request handler waiting to enqueue its own
    // log entry (HandlerView::handle, line 262).
    std::vector<LoggedRequest> to_flush;
    LogListType cleared_owner;  // takes ownership of the cleared queue
    bool transitioned_off = false;

    {
      WriteLock lock(itsLoggingMutex);

      const bool previousStatus = isLogging;
      isLogging = newStatus;

      if (isLogging == previousStatus)
      {
        // No change in status, simply return.
        return;
      }

      if (!isLogging)
      {
        // True -> false: snapshot pending entries for one final
        // flush, then move the in-memory queue into a local list
        // so its destructors run AFTER the lock is released.
        auto first = firstUnflushed(itsRequestLog, itsLastFlushedRequest);
        if (first != itsRequestLog.end())
          to_flush.assign(first, itsRequestLog.end());
        cleared_owner.splice(cleared_owner.end(), itsRequestLog);
        itsLastFlushedRequest = itsRequestLog.end();
        transitioned_off = true;
      }
      else
      {
        // False -> true: nothing to flush; reset the marker and
        // (re-)open the on-disk file. itsAccessLog->start() opens
        // an ofstream which is a quick syscall but does no I/O
        // beyond create-if-missing, so it stays under the lock.
        itsLastFlushedRequest = itsRequestLog.end();
        itsAccessLog->start();
      }
    }

    if (!to_flush.empty())
    {
      for (const auto& r : to_flush)
        itsAccessLog->log(r);
      itsAccessLog->flush();
      if (itsOTelLog)
        itsOTelLog->flush();
    }
    if (transitioned_off)
      itsAccessLog->stop();
    // cleared_owner destructs here, outside the lock.
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool HandlerView::isCatchNoMatch() const
{
  return itsIsCatchNoMatch;
}

bool HandlerView::queryIsFast(HTTP::Request& theRequest) const
{
  try
  {
    // Assume that all requests with no plugin are fast queries
    // Otherwise ask the plugin
    return !itsPlugin || itsPlugin->queryIsFast(theRequest);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool HandlerView::isAdminQuery(HTTP::Request& theRequest) const
{
  try
  {
    // Assume that all requests with no plugin are admin queries
    // Otherwise ask the plugin if it is an admin query
    return !itsPlugin || itsPlugin->isAdminQuery(theRequest);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string HandlerView::getPluginName() const
{
  try
  {
    return itsPlugin ? itsPlugin->getPluginName() : "<builtin>";
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool HandlerView::getLogging() const
{
  ReadLock lock(itsLoggingMutex);
  return isLogging;
}

// Periodic cleaner driven from ContentHandlerMap::cleanLog (every
// 5 s). The previous implementation held the WriteLock for the
// duration of (a) the disk flush — slow, sometimes seconds under
// concurrent reader load — and (b) the destruction of N
// LoggedRequest objects (each carrying ~10 std::string members).
// Both are now off the critical path.
//
// Phase A: under a brief WriteLock, copy entries pending flush into
//          a local vector and advance ``itsLastFlushedRequest``.
//          The lock is released immediately afterwards.
// Phase B: write the local vector to disk with NO LOCK HELD.
//          Concurrent request handlers can ``push_back`` freely.
// Phase C: under another brief WriteLock, splice the to-be-purged
//          prefix into a local list. Drop the lock. The local
//          list destructs outside the critical section so request
//          handlers don't queue behind N std::string frees.
void HandlerView::cleanLog(const Fmi::DateTime& minTime, bool flush)
{
  try
  {
    // ----- Phase A: snapshot entries pending flush -----
    std::vector<LoggedRequest> to_flush;
    if (flush && itsAccessLog)
    {
      WriteLock lock(itsLoggingMutex);
      auto first = firstUnflushed(itsRequestLog, itsLastFlushedRequest);
      if (first != itsRequestLog.end())
      {
        to_flush.assign(first, itsRequestLog.end());
        itsLastFlushedRequest = std::prev(itsRequestLog.end());
      }
    }

    // ----- Phase B: disk I/O outside the lock -----
    if (!to_flush.empty())
    {
      for (const auto& r : to_flush)
        itsAccessLog->log(r);
      itsAccessLog->flush();
      if (itsOTelLog)
        itsOTelLog->flush();
    }

    // ----- Phase C: purge old entries; destructors run unlocked -----
    LogListType erased_owner;
    {
      WriteLock lock(itsLoggingMutex);

      // Skip the purge if a LogRange (?what=lastrequests) is in
      // flight; it holds raw iterators we mustn't invalidate. The
      // entries will be picked up on the next cycle.
      if (itsLogReaderCount != 0)
        return;

      auto purge_end = std::find_if(itsRequestLog.begin(),
                                     itsRequestLog.end(),
                                     std::bind(isNotOld, minTime, p::_1));

      if (purge_end != itsRequestLog.begin())
      {
        // After Phase A, ``itsLastFlushedRequest`` points to the
        // most recent entry. If we are erasing every entry
        // (purge_end == end()), the marker becomes invalid; reset
        // to end(). Otherwise the most recent entry survives and
        // the iterator remains valid (std::list iterator stability).
        if (purge_end == itsRequestLog.end())
          itsLastFlushedRequest = itsRequestLog.end();

        // splice() transfers nodes between lists in O(N) (size
        // bookkeeping) without allocating, copying, or destructing.
        // The destructors then fire when ``erased_owner`` goes out
        // of scope below — outside the WriteLock.
        erased_owner.splice(erased_owner.end(),
                             itsRequestLog,
                             itsRequestLog.begin(),
                             purge_end);
      }
    }
    // erased_owner destructs here, outside the lock.
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Operator-driven flush (e.g. handler destructor on shutdown). Same
// lock-vs-IO split as cleanLog's Phase A + B; no purge phase.
void HandlerView::flushLog()
{
  try
  {
    std::vector<LoggedRequest> to_flush;
    {
      WriteLock lock(itsLoggingMutex);
      if (itsAccessLog == nullptr)
        return;
      auto first = firstUnflushed(itsRequestLog, itsLastFlushedRequest);
      if (first != itsRequestLog.end())
      {
        to_flush.assign(first, itsRequestLog.end());
        itsLastFlushedRequest = std::prev(itsRequestLog.end());
      }
    }
    if (!to_flush.empty())
    {
      for (const auto& r : to_flush)
        itsAccessLog->log(r);
      itsAccessLog->flush();
      if (itsOTelLog)
        itsOTelLog->flush();
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

LogRange HandlerView::getLoggedRequests()
{
  ReadLock lock(itsLoggingMutex);
  lockLogRange();
  return {itsRequestLog, this};
}

void HandlerView::releaseLogRange()
{
  --itsLogReaderCount;
}

void HandlerView::lockLogRange()
{
  ++itsLogReaderCount;
}

bool HandlerView::usesPlugin(const SmartMetPlugin* plugin) const
{
  return plugin == itsPlugin;
}

const std::string& HandlerView::getResource() const
{
  return itsResource;
}

}  // namespace Spine
}  // namespace SmartMet
