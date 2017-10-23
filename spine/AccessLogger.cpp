#include "AccessLogger.h"
#include "Exception.h"

#include <boost/filesystem.hpp>
#include <macgyver/StringConversion.h>

namespace
{
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

}  // namespace Spine
}  // namespace SmartMet
