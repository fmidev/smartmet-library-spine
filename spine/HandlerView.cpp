#include "HandlerView.h"
#include "Exception.h"
#include "Reactor.h"

#include <macgyver/StringConversion.h>

#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>

namespace
{
bool isNotOld(const boost::posix_time::ptime& target, const SmartMet::Spine::LoggedRequest& compare)
{
  return compare.getRequestTime() > target;
}

std::string makeAccessLogFileName(const std::string& resource, const std::string& accessLogDir)
{
  try
  {
    // Build access log full path

    // Resource must be at least 1 character long
    std::string resourceId = resource.substr(1);

    boost::filesystem::path filepath(accessLogDir);
    if (resourceId.size() > 0)
    {
      boost::algorithm::replace_all(resourceId, "/", "-");  // this in case there are subhandlers
      filepath /= resourceId + "-access-log";
    }
    else
      filepath /= "default-handler-access-log";

    return filepath.string();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::unique_ptr<std::ofstream> makeAccessLogFile(const std::string& resource,
                                                 const std::string& accessLogDir)
{
  try
  {
    std::string path = ::makeAccessLogFileName(resource, accessLogDir);

    std::unique_ptr<std::ofstream> file(new std::ofstream());
    file->open(path, std::ofstream::out | std::ofstream::app);
    if (!file->is_open())
    {
      throw SmartMet::Spine::Exception(BCP, "Could not open access log file: " + path);
    }

    return std::move(file);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
}

namespace SmartMet
{
namespace Spine
{
AccessLogger::AccessLogger(const std::string& resource, const std::string& accessLogDir)
    : itsResource(resource), itsLoggingDir(accessLogDir), itsIsRunning(false)
{
}

void AccessLogger::start()
{
  try
  {
    itsFileHandle = ::makeAccessLogFile(itsResource, itsLoggingDir);

    itsIsRunning = true;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void AccessLogger::stop()
{
  try
  {
    if (itsFileHandle && itsFileHandle->is_open())
      itsFileHandle->close();

    itsIsRunning = false;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

AccessLogger::~AccessLogger()
{
  this->stop();
}

void AccessLogger::log(const LoggedRequest& theRequest)
{
  try
  {
    if (!itsIsRunning || !itsFileHandle)
    {
      // Trying to log into a stopped logger
      return;
    }

    *itsFileHandle << theRequest.getIP() << " - - ["
                   << Fmi::to_iso_extended_string(theRequest.getRequestTime()) << "]"
                   << " \"" << theRequest.getMethod() << " " << theRequest.getRequestString()
                   << " HTTP/" << theRequest.getVersion() << "\" " << theRequest.getStatus() << " ("
                   << theRequest.getAccessDuration().total_milliseconds() << " ms)" << std::endl;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

HandlerView::HandlerView(ContentHandler theHandler,
                         boost::shared_ptr<IPFilter::IPFilter> theIpFilter,
                         SmartMetPlugin* thePlugin,
                         const std::string& theResource,
                         bool loggingStatus,
                         const std::string& accessLogDir)
    : itsHandler(theHandler),
      itsIpFilter(theIpFilter),
      itsPlugin(thePlugin),
      itsResource(theResource),
      itsIsCatchNoMatch(false),
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

HandlerView::HandlerView(ContentHandler theHandler)
    : itsHandler(theHandler),
      itsIsCatchNoMatch(true),
      isLogging(false),
      itsLastFlushedRequest(itsRequestLog.begin())
{
}

HandlerView::~HandlerView()
{
}

bool HandlerView::handle(Reactor& theReactor,
                         const HTTP::Request& theRequest,
                         HTTP::Response& theResponse)
{
  try
  {
    if (!itsIsCatchNoMatch)
    {
      if (itsIpFilter != 0)
      {
        if (!itsIpFilter->match(theRequest.getClientIP()))
        {
          // False means ip filter blocked this
          return false;
        }
      }
    }

    if (isLogging && !itsIsCatchNoMatch)  // Disable frontend logging always
    {
      auto before = boost::posix_time::microsec_clock::universal_time();
      itsHandler(theReactor, theRequest, theResponse);
      auto accessDuration = boost::posix_time::microsec_clock::universal_time() - before;

      WriteLock lock(itsLoggingMutex);
      if (isLogging)  // Need to check this again because it may have changed in previous request
      {
        // Insert new request to the logging list
        itsRequestLog.emplace_back(LoggedRequest(theRequest.getURI(),
                                                 boost::posix_time::microsec_clock::local_time(),
                                                 accessDuration,
                                                 theResponse.getStatusString(),
                                                 theRequest.getClientIP(),
                                                 theRequest.getMethodString(),
                                                 theResponse.getVersion()));
      }
    }
    else
    {
      itsHandler(theReactor, theRequest, theResponse);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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

    LogListType::iterator it = std::find_if(
        itsRequestLog.begin(), itsRequestLog.end(), boost::bind(isNotOld, minTime, _1));

    // Update disk flush iterator accordingly

    // Find distance between flushed request iterator and purge iterator

    LogListType::iterator iter = itsLastFlushedRequest;

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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

LogListType HandlerView::getLoggedRequests()
{
  ReadLock lock(itsLoggingMutex);
  return itsRequestLog;
}

}  // namespace Spine
}  // namespace SmartMet
