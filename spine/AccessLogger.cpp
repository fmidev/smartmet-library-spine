#include "AccessLogger.h"
#include "Convenience.h"
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
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::unique_ptr<std::ofstream> makeAccessLogFile(const std::string& resource,
                                                 const std::string& accessLogDir)
{
  try
  {
    // Create the log directory if it is missing
    if (!boost::filesystem::exists(accessLogDir))
    {
      std::cout << SmartMet::Spine::log_time_str() << " creating access log directory "
                << accessLogDir << std::endl;
      boost::filesystem::create_directories(accessLogDir);
    }

    // Error if the path is not a directory
    if (!boost::filesystem::is_directory(accessLogDir))
      throw std::runtime_error("Access log path '" + accessLogDir + "' is not a directory");

    // Otherwise create a log file in the directory
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
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
                   << Fmi::to_iso_extended_string(theRequest.getRequestEndTime()) << "] \""
                   << theRequest.getMethod() << ' ' << theRequest.getRequestString() << " HTTP/"
                   << theRequest.getVersion() << "\" " << theRequest.getStatus() << " ["
                   << Fmi::to_iso_extended_string(theRequest.getRequestStartTime()) << "] "
                   << theRequest.getAccessDuration().total_milliseconds() << std::endl;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
