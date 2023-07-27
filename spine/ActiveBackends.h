#pragma once

#include <map>
#include <string>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Track number of requests to backends
 */
// ----------------------------------------------------------------------

class ActiveBackends
{
 public:
  ActiveBackends() = default;
  ActiveBackends(const ActiveBackends& other) = delete;
  ActiveBackends(ActiveBackends&& other) = delete;
  ActiveBackends& operator=(const ActiveBackends& other) = delete;
  ActiveBackends& operator=(ActiveBackends&& other) = delete;

  using PortCounter = std::map<int, int>;
  using Status = std::map<std::string, PortCounter>;

  void start(const std::string& theHost, int thePort);
  void stop(const std::string& theHost, int thePort);
  void reset(const std::string& theHost, int thePort);
  void remove(const std::string& theHost, int thePort);

  Status status() const;

 private:
  Status itsStatus;
};
}  // namespace Spine
}  // namespace SmartMet
