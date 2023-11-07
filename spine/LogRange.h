#pragma once

#include "LoggedRequest.h"
#include <map>
#include <string>

namespace SmartMet
{
namespace Spine
{
class HandlerView;

// A proxy object to pass a log range to users and then to release the range when done
// so that log cleaning can proceed

class LogRange
{
 public:
  ~LogRange();
  LogRange(const LogListType& theLog, HandlerView* theHandler);

  LogRange(const LogRange& theOther);
  LogRange() = delete;

  using const_iterator = LogListType::const_iterator;

  const_iterator begin() const;
  const_iterator end() const;

 private:
  const LogListType::const_iterator itsBegin;
  const LogListType::const_iterator itsEnd;
  HandlerView* itsHandlerView = nullptr;
};

// Logging-related typedefs
using LoggedRequests = std::map<std::string, LogRange>;

// Fields are: Logging enabled flag, the logged requests, last cleanup time
using AccessLogStruct = std::tuple<bool, LoggedRequests, Fmi::DateTime>;

}  // namespace Spine
}  // namespace SmartMet
