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
  AccessLogger(std::string resource, std::string accessLogDir);

  ~AccessLogger();

  void log(const LoggedRequest& theRequest);

  void start();

  void stop();

 private:
  std::string itsResource;

  std::string itsLoggingDir;

  std::unique_ptr<std::ofstream> itsFileHandle;

  bool itsIsRunning = false;
};

}  // namespace Spine
}  // namespace SmartMet
