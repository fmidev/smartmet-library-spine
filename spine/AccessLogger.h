#pragma once

#include "LoggedRequest.h"
#include <fstream>
#include <list>
#include <string>

namespace SmartMet
{
namespace Spine
{
// AccessLogger handles a file for access logs and rotation
class AccessLogger
{
 public:
  AccessLogger(const std::string& resource, const std::string& accessLogDir);

  ~AccessLogger();

  void log(const LoggedRequest& theRequest);

  void start();

  void stop();

 private:
  std::string itsResource;

  std::string itsLoggingDir;

  std::unique_ptr<std::ofstream> itsFileHandle;

  bool itsIsRunning;
};

}  // namespace Spine
}  // namespace SmartMet
