#include "SmartMetPlugin.h"
#include "Convenience.h"
#include "Reactor.h"
#include <boost/thread.hpp>
#include <boost/timer/timer.hpp>
#include <macgyver/Exception.h>

SmartMetPlugin::SmartMetPlugin() = default;

// ----------------------------------------------------------------------
/*!
 * \brief Default performance query implementation. If not overrided in
 * the actual plugin, handler calls are scheduled to the slow pool
 */
// ----------------------------------------------------------------------

bool SmartMetPlugin::queryIsFast(const SmartMet::Spine::HTTP::Request & /* theRequest */) const
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
      if (!SmartMet::Spine::Reactor::isShuttingDown())
        init();
    }
    catch (...)
    {
      Fmi::Exception exception(BCP, "Init call failed!", nullptr);
      if (!SmartMet::Spine::Reactor::isShuttingDown())
      {
        itsInitActive = false;
        throw exception;
      }
      // else
      //  std::cout << "SHUTDOWN EXCEPTION :\n" << exception.what() << "\n";
    }

    itsInitActive = false;
  }
  catch (...)
  {
    Fmi::Exception exception(BCP, "Plugin initialization failed!", nullptr);

    // Will terminate the program
    throw exception;
  }
}

bool SmartMetPlugin::isInitActive()
{
  return itsInitActive;
}

void SmartMetPlugin::shutdownPlugin()
{
  try
  {
    // Calling the plugin specific shutdown() -method.
    shutdown();

    while (itsInitActive)
    {
      std::cout << ("  -- waiting the plugin (" + getPluginName() +
                    ") to complete its initialization phase\n")
                << std::flush;
      boost::this_thread::sleep(boost::posix_time::milliseconds(3000));
    }

    while (responseCounter < requestCounter)
    {
      std::cout << ("  -- waiting the plugin (" + getPluginName() +
                    ") to complete its processing (" + std::to_string(responseCounter) + "/" +
                    std::to_string(requestCounter) + "\n")
                << std::flush;
      boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    if (SmartMet::Spine::Reactor::isShuttingDown() || itsInitActive)
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
    throw Fmi::Exception::Trace(BCP, "Plugin request handler failed!")
        .addParameter("request start time (UTC)", Fmi::to_iso_string(now));
  }
}
