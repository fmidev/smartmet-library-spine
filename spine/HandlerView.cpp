#include "HandlerView.h"
#include "Convenience.h"
#include "FmiApiKey.h"
#include "Reactor.h"
#include <iostream>
#include <boost/bind/bind.hpp>
#include <filesystem>
#include <macgyver/DateTime.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>

namespace
{
bool isNotOld(const Fmi::DateTime& target, const SmartMet::Spine::LoggedRequest& compare)
{
  return compare.getRequestEndTime() > target;
}
}  // namespace

using namespace boost::placeholders;

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
                         const std::string& accessLogDir)
    : itsHandler(std::move(theHandler)),
      itsIpFilter(std::move(theIpFilter)),
      itsPlugin(thePlugin),
      itsResource(theResource),
      itsPrivate(isprivate),
      isLogging(loggingStatus),
      itsLastFlushedRequest(itsRequestLog.begin()),
      itsAccessLog(new AccessLogger(theResource, accessLogDir))
{
  try
  {
    if (isLogging)
      itsAccessLog->start();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

HandlerView::HandlerView(ContentHandler theHandler)
    : itsHandler(std::move(theHandler)),
      itsIsCatchNoMatch(true),
      itsLastFlushedRequest(itsRequestLog.begin())
{
}

HandlerView::~HandlerView() = default;

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

    if (itsIsCatchNoMatch || !isLogging)
    {
      // Frontends do not log finished requests
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
      auto key = theReactor.insertActiveRequest(theRequest);
      auto before = Fmi::MicrosecClock::universal_time();

      std::exception_ptr error;
      try
      {
        itsHandler(theReactor, theRequest, theResponse);
      }
      catch (...)
      {
        error = std::current_exception();
      }
      auto accessDuration = Fmi::MicrosecClock::universal_time() - before;
      theReactor.removeActiveRequest(key, theResponse.getStatus());

      WriteLock lock(itsLoggingMutex);
      if (isLogging)  // Need to check this again because it may have changed in previous request
      {
        // Insert new request to the logging list

        auto etag = theResponse.getHeader("ETag");
        auto apikey = FmiApiKey::getFmiApiKey(theRequest);

        itsRequestLog.emplace_back(LoggedRequest(theRequest.getURI(),
                                                 Fmi::MicrosecClock::local_time(),
                                                 accessDuration,
                                                 theResponse.getStatusString(),
                                                 theRequest.getClientIP(),
                                                 theRequest.getMethodString(),
                                                 theResponse.getVersion(),
                                                 theResponse.getContentLength(),
                                                 (etag ? *etag : "-"),
                                                 (apikey ? *apikey : "-")));
      }

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
    WriteLock lock(itsLoggingMutex);

    // Disable logging-related stuff for frontend behaviour
    // Access log pointer is empty and will segfault if we proceed
    if (itsIsCatchNoMatch)
    {
      return;
    }

    bool previousStatus = isLogging;  // Save previous status to detect changes
    isLogging = newStatus;

    if (isLogging == previousStatus)
    {
      // No change in status, simply return
      return;
    }

    if (!isLogging)
    {
      // Status set to false, make the transition true->false

      // Empty whatever we may have
      itsRequestLog.clear();
      itsLastFlushedRequest = itsRequestLog.begin();
      itsAccessLog->stop();
    }
    else
    {
      // Status set to true, make the transition false->true
      itsLastFlushedRequest = itsRequestLog.begin();
      itsAccessLog->start();
    }
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
    return itsPlugin->queryIsFast(theRequest);
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
    return itsPlugin->isAdminQuery(theRequest);
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
    return itsPlugin->getPluginName();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool HandlerView::getLogging()
{
  ReadLock lock(itsLoggingMutex);
  return isLogging;
}

void HandlerView::cleanLog(const Fmi::DateTime& minTime)
{
  try
  {
    WriteLock lock(itsLoggingMutex);

    // Disable logging-related things for frontend behaviour
    if (itsIsCatchNoMatch)
      return;

    // Ignore cleaning operation if someone is reading the log right now

    if (itsLogReaderCount != 0)
      return;

    auto it = std::find_if(
        itsRequestLog.begin(), itsRequestLog.end(), boost::bind(isNotOld, minTime, _1));

    // Update disk flush iterator accordingly

    // Find distance between flushed request iterator and purge iterator

    auto iter = itsLastFlushedRequest;

    while (iter != itsRequestLog.end())
    {
      if (iter == it)
      {
        // Log purging iterator has passed the last flushed iterator (should happen only with very
        // tight log cleaning)
        itsLastFlushedRequest = it;
        break;
      }

      ++iter;
    }

    itsRequestLog.erase(itsRequestLog.begin(), it);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void HandlerView::flushLog()
{
  try
  {
    WriteLock lock(itsLoggingMutex);

    // Disable logging-related things for frontend behaviour
    if (itsIsCatchNoMatch)
    {
      return;
    }

    auto flushIter = itsLastFlushedRequest;
    ++flushIter;

    for (; flushIter != itsRequestLog.end(); ++flushIter)
    {
      itsAccessLog->log(*flushIter);
    }

    // Return to the last flushed request (not past the end)
    itsLastFlushedRequest = --flushIter;
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
