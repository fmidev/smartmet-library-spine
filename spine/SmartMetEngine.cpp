#include "SmartMetEngine.h"
#include "Convenience.h"
#include "Exception.h"
#include "Reactor.h"
#include <boost/timer/timer.hpp>
#include <macgyver/AnsiEscapeCodes.h>
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
    boost::unique_lock<boost::mutex> theLock(itsInitMutex);

    this->init();

    isReady = true;
    itsCond.notify_all();
  }
  catch (...)
  {
    throw Exception::Trace(BCP, "Engine construction failed!");
  }
}

void SmartMetEngine::wait()
{
  try
  {
    boost::unique_lock<boost::mutex> theLock(itsInitMutex);
    if (!isReady)
    {
      itsCond.wait(theLock);
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void SmartMetEngine::setShutdownRequestedFlag()
{
  try
  {
    itsShutdownRequested = true;
    shutdownRequestFlagSet();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void SmartMetEngine::shutdownEngine()
{
  try
  {
    itsShutdownRequested = true;
    shutdown();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void SmartMetEngine::shutdownRequestFlagSet()
{
  // This method can be overridden if a plugin wants to be informed when the
  // shutdownRequestedFlag is set.
}

}  // namespace Spine
}  // namespace SmartMet
