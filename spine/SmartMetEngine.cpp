#include "SmartMetEngine.h"
#include "Convenience.h"
#include "Reactor.h"
#include <boost/timer/timer.hpp>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/Exception.h>
#include <sys/types.h>
#include <csignal>
#include <iostream>

namespace SmartMet
{
namespace Spine
{
SmartMetEngine::~SmartMetEngine() = default;

void SmartMetEngine::construct(const std::string& /* engineName */, Reactor* reactor)
{
  try
  {
    itsReactor = reactor;

    this->init();

    isReady = true;
    itsCond.notify_all();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Engine construction failed!");
  }
}

void SmartMetEngine::wait()
{
  try
  {
    boost::unique_lock<boost::mutex> theLock(itsInitMutex);
    while (!isReady && !itsShutdownRequested) {
      itsCond.wait_for(
        theLock,
        boost::chrono::seconds(1),
        [this]() -> bool { return isReady || itsShutdownRequested; });
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void SmartMetEngine::setShutdownRequestedFlag()
{
  try
  {
    itsShutdownRequested = true;
    shutdownRequestFlagSet();
    itsCond.notify_all();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void SmartMetEngine::shutdownEngine()
{
  try
  {
    itsShutdownRequested = true;
    itsCond.notify_all();
    shutdown();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void SmartMetEngine::shutdownRequestFlagSet()
{
  // This method can be overridden if a plugin wants to be informed when the
  // shutdownRequestedFlag is set.
}

}  // namespace Spine
}  // namespace SmartMet
