#include "AccessLogger.h"
#include "Convenience.h"
#include <iostream>
#include <filesystem>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>

namespace
{
std::string makeAccessLogFileName(const std::string& resource, const std::string& accessLogDir)
{
  try
  {
    // Remove leading and trailing "/" characters
    auto id = resource;
    while (!id.empty() && id.front() == '/')
      id = id.substr(1);
    while (!id.empty() && id.back() == '/')
      id = id.substr(0, id.size() - 1);

    // Build access log full path
    std::filesystem::path filepath(accessLogDir);

    if (!id.empty())
    {
      boost::algorithm::replace_all(id, "/", "-");  // this in case there are subhandlers
      filepath /= id + "-access-log";
    }
    else
      filepath /= "default-handler-access-log";

    return filepath.string();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::unique_ptr<std::ofstream> makeAccessLogFile(const std::string& resource,
                                                 const std::string& accessLogDir)
{
  try
  {
    // Create the log directory if it is missing
    if (!std::filesystem::exists(accessLogDir))
    {
      std::cout << SmartMet::Spine::log_time_str() << " creating access log directory "
                << accessLogDir << std::endl;
      std::filesystem::create_directories(accessLogDir);
    }

    // Error if the path is not a directory
    if (!std::filesystem::is_directory(accessLogDir))
      throw std::runtime_error("Access log path '" + accessLogDir + "' is not a directory");

    // Otherwise create a log file in the directory
    std::string path = ::makeAccessLogFileName(resource, accessLogDir);

    std::unique_ptr<std::ofstream> file(new std::ofstream());
    file->open(path, std::ofstream::out | std::ofstream::app);
    if (!file->is_open())
    {
      throw Fmi::Exception(BCP, "Could not open access log file: " + path);
    }

    return file;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

namespace SmartMet
{
namespace Spine
{
AccessLogger::AccessLogger(std::string resource, std::string accessLogDir)
    : itsResource(std::move(resource)), itsLoggingDir(std::move(accessLogDir))
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

AccessLogger::~AccessLogger()
{
  try
  {
    this->stop();
  }
  catch (...)
  {
    std::cout << Fmi::Exception::Trace(BCP, "Operation failed!").getStackTrace() << std::endl;
  }
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
                   << theRequest.getAccessDuration().total_milliseconds() << ' '
                   << theRequest.getContentLength() << ' ' << theRequest.getETag() << ' '
                   << theRequest.getApiKey() << '\n';
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
