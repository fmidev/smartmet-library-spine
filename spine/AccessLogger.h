#pragma once
#include <fstream>
#include <memory>
#include <string>

namespace SmartMet::Spine
{
class LoggedRequest;
}  // namespace SmartMet::Spine

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
  AccessLogger(const AccessLogger&) = delete;
  AccessLogger& operator=(const AccessLogger&) = delete;

  void log(const LoggedRequest& theRequest);

  void start();

  void stop();

  void flush();

 private:
  std::string itsResource;

  std::string itsLoggingDir;

  std::unique_ptr<std::ofstream> itsFileHandle;

  bool itsIsRunning = false;
};

}  // namespace Spine
}  // namespace SmartMet
