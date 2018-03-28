#include "SmartMetPlugin.h"
#include "Convenience.h"
#include "Exception.h"
#include <boost/thread.hpp>
#include <boost/timer/timer.hpp>

SmartMetPlugin::SmartMetPlugin()
    : itsInitActive(false), itsShutdownRequested(false), requestCounter(0), responseCounter(0)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

SmartMetPlugin::~SmartMetPlugin() {}
// ----------------------------------------------------------------------
/*!
 * \brief Default performance query implementation. If not overrided in
 * the actual plugin, handler calls are scheduled to the slow pool
 */
// ----------------------------------------------------------------------

bool SmartMetPlugin::queryIsFast(const SmartMet::Spine::HTTP::Request &) const
{
  return false;
}
// ======================================================================

void SmartMetPlugin::initPlugin()
{
  try
  {
    itsInitActive = true;

    try
    {
      if (!itsShutdownRequested)
        init();
    }
    catch (...)
    {
      SmartMet::Spine::Exception exception(BCP, "Init call failed!", nullptr);
      if (!itsShutdownRequested)
        throw exception;
      // else
      //  std::cout << "SHUTDOWN EXCEPTION :\n" << exception.what() << "\n";
    }

    itsInitActive = false;
  }
  catch (...)
  {
    SmartMet::Spine::Exception exception(BCP, "Plugin initialization failed!", nullptr);

    if (!exception.stackTraceDisabled())
      std::cerr << exception.getStackTrace();
    else if (!exception.loggingDisabled())
      std::cerr << SmartMet::Spine::log_time_str() + " Error: " + exception.what() << std::endl;

    // Will terminate the program
    throw exception;
  }
}

bool SmartMetPlugin::isInitActive()
{
  return itsInitActive;
}
bool SmartMetPlugin::isShutdownRequested()
{
  return itsShutdownRequested;
}
void SmartMetPlugin::setShutdownRequestedFlag()
{
  itsShutdownRequested = true;
}
void SmartMetPlugin::shutdownPlugin()
{
  try
  {
    itsShutdownRequested = true;

    // Calling the plugin specific shutdown() -method.
    shutdown();

    while (itsInitActive)
    {
      std::cout << "  -- waiting the plugin (" << getPluginName()
                << ") to complete its initialization phase\n";
      boost::this_thread::sleep(boost::posix_time::milliseconds(3000));
    }

    while (responseCounter < requestCounter)
    {
      std::cout << "  -- waiting the plugin (" << getPluginName()
                << ") to complete its processing (" << responseCounter << "/" << requestCounter
                << ")\n";
      boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void SmartMetPlugin::callRequestHandler(SmartMet::Spine::Reactor &theReactor,
                                        const SmartMet::Spine::HTTP::Request &theRequest,
                                        SmartMet::Spine::HTTP::Response &theResponse)
{
  // This variable is created only to make analyzing core dumps easier.
  // We use the variable in the exception only to silence compiler warnings
  // on unused variables.
  auto now = boost::posix_time::second_clock::universal_time();

  try
  {
    if (itsShutdownRequested || itsInitActive)
    {
      theResponse.setStatus(SmartMet::Spine::HTTP::Status::service_unavailable);
      return;
    }

#ifdef DEBUG
    // Printing the request processing time:
    std::string report =
        std::string("* Request processed [" + getPluginName() + "] in %t sec CPU, %w sec real\n\n");
    boost::timer::auto_cpu_timer timer(2, report);
#endif

    requestCounter++;
    requestHandler(theReactor, theRequest, theResponse);
    responseCounter++;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception::Trace(BCP, "Plugin request handler failed!")
        .addParameter("request start time (UTC)", Fmi::to_iso_string(now));
  }
}
