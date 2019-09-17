#pragma once

#include <boost/noncopyable.hpp>
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

class ActiveBackends : private boost::noncopyable
{
 public:
  using PortCounter = std::map<int, int>;
  using Status = std::map<std::string, PortCounter>;

  void start(const std::string& theHost, int thePort);
  void stop(const std::string& theHost, int thePort);
  void reset(const std::string& theHost, int thePort);

  Status status() const;

 private:
  Status itsStatus;
};
}  // namespace Spine
}  // namespace SmartMet
