#include "HandlerView.h"
#include "Convenience.h"
#include "Exception.h"
#include "Reactor.h"
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <macgyver/StringConversion.h>

namespace
{
bool isNotOld(const boost::posix_time::ptime& target, const SmartMet::Spine::LoggedRequest& compare)
{
  return compare.getRequestEndTime() > target;
}
}  // namespace

namespace SmartMet
{
namespace Spine
{
HandlerView::HandlerView(ContentHandler theHandler,
                         boost::shared_ptr<IPFilter::IPFilter> theIpFilter,
                         SmartMetPlugin* thePlugin,
                         const std::string& theResource,
                         bool loggingStatus,
                         bool itsPrivate,
                         const std::string& accessLogDir)
    : itsHandler(std::move(theHandler)),
      itsIpFilter(std::move(theIpFilter)),
      itsPlugin(thePlugin),
      itsResource(theResource),
      itsIsCatchNoMatch(false),
      itsPrivate(itsPrivate),
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

HandlerView::HandlerView(ContentHandler theHandler)
    : itsHandler(std::move(theHandler)),
      itsIsCatchNoMatch(true),
      isLogging(false),
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
      itsHandler(theReactor, theRequest, theResponse);
      theReactor.removeActiveRequest(key, theResponse.getStatus());
    }
    else
    {
      auto key = theReactor.insertActiveRequest(theRequest);
      auto before = boost::posix_time::microsec_clock::universal_time();
      itsHandler(theReactor, theRequest, theResponse);
      auto accessDuration = boost::posix_time::microsec_clock::universal_time() - before;
      theReactor.removeActiveRequest(key, theResponse.getStatus());

      WriteLock lock(itsLoggingMutex);
      if (isLogging)  // Need to check this again because it may have changed in previous request
      {
        // Insert new request to the logging list

        auto etag = theResponse.getHeader("ETag");

        itsRequestLog.emplace_back(LoggedRequest(theRequest.getURI(),
                                                 boost::posix_time::microsec_clock::local_time(),
                                                 accessDuration,
                                                 theResponse.getStatusString(),
                                                 theRequest.getClientIP(),
                                                 theRequest.getMethodString(),
                                                 theResponse.getVersion(),
                                                 theResponse.getContentLength(),
                                                 (etag ? *etag : "-")));
      }
    }

    return true;
  }
  catch (...)
  {
    std::cerr << "Operation failed! HandlerView::handle aborted" << std::endl;
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

bool HandlerView::getLogging()
{
  ReadLock lock(itsLoggingMutex);
  return isLogging;
}

void HandlerView::cleanLog(const boost::posix_time::ptime& minTime)
{
  try
  {
    WriteLock lock(itsLoggingMutex);

    // Disable logging-related things for frontend behaviour
    if (itsIsCatchNoMatch)
    {
      return;
    }

    // Ignore cleaning operation if someone is reading the log right now

    if (itsLogReaderCount != 0)
    {
      std::cout << log_time_str() << " not cleaning logs since someone is reading them"
                << std::endl;
      return;
    }

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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

LogRange HandlerView::getLoggedRequests()
{
  ReadLock lock(itsLoggingMutex);
  lockLogRange();
  return LogRange(itsRequestLog, this);
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
