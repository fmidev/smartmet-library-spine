#include "SmartMetEngine.h"
#include "Convenience.h"
#include "Exception.h"
#include "Reactor.h"
#include <macgyver/AnsiEscapeCodes.h>
#include <sys/types.h>
#include <iostream>
#include <signal.h>

#include <boost/timer/timer.hpp>

namespace SmartMet
{
namespace Spine
{
SmartMetEngine::SmartMetEngine() : itsShutdownRequested(false), isReady(false) {}

SmartMetEngine::~SmartMetEngine() {}

void SmartMetEngine::construct(const std::string& engineName, Reactor* reactor)
{
  try
  {
    boost::unique_lock<boost::mutex> theLock(itsInitMutex);

    std::string report = (std::string(ANSI_FG_GREEN) + "Engine [" + engineName +
                          "] initialized in %t sec CPU, %w sec real \n" + ANSI_FG_DEFAULT);
    boost::timer::auto_cpu_timer timer(2, report);

    this->init();

    isReady = true;
    itsCond.notify_all();
    reactor->engineInitializedCallback(this);
  }
  catch (...)
  {
    SmartMet::Spine::Exception exception(BCP, "Engine construction failed!", NULL);

    if (!exception.stackTraceDisabled())
      std::cerr << exception.getStackTrace();
    else if (!exception.loggingDisabled())
      std::cerr << Spine::log_time_str() + " Error: " + exception.what() << std::endl;

    kill(getpid(), SIGKILL);  // If we use exit() we might get a core dump.
                              // exit(-1);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void SmartMetEngine::shutdownRequestFlagSet()
{
  // This method can be overridden if a plugin wants to be informed when the
  // shutdownRequestedFlag is set.
}

}  // namespace Spine
}  // namespace SmartMet
