#include "SmartMetPlugin.h"
#include "Convenience.h"
#include "HTTP.h"
#include "Reactor.h"
#include <iostream>
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

// ----------------------------------------------------------------------
/*!
 * \brief Default admin implementation. If not overrided in
 * the actual plugin, handler calls are scheduled to the slow/fast pools
 */
// ----------------------------------------------------------------------

bool SmartMetPlugin::isAdminQuery(const SmartMet::Spine::HTTP::Request & /* theRequest */) const
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
    using SmartMet::Spine::Reactor;
    Reactor::reportFailure("Plugin initialization failed!");
    Fmi::Exception exception(BCP, "Plugin initialization failed!", nullptr);

    // Will terminate the program
    throw exception;
  }
}

bool SmartMetPlugin::isInitActive() const
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
      boost::this_thread::sleep_for(boost::chrono::milliseconds(3000));
    }

    while (responseCounter < requestCounter)
    {
      std::cout << ("  -- waiting the plugin (" + getPluginName() +
                    ") to complete its processing (" + std::to_string(responseCounter) + "/" +
                    std::to_string(requestCounter) + "\n")
                << std::flush;
      boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
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
  auto now = Fmi::SecondClock::universal_time();

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

bool SmartMetPlugin::checkRequest(const SmartMet::Spine::HTTP::Request &theRequest,
                                  SmartMet::Spine::HTTP::Response &theResponse,
                                  bool supportsPost)
{
  try
  {
    using namespace SmartMet::Spine;
    switch (theRequest.getMethod())
    {
      case HTTP::RequestMethod::GET:
        return false;

      case HTTP::RequestMethod::POST:
        if (supportsPost)
        {
          return false;
        }
        else
        {
          theResponse.setStatus(HTTP::not_found);
          return true;
        }

      case HTTP::RequestMethod::OPTIONS:
        if (supportsPost)
        {
          theResponse = HTTP::Response::stockOptionsResponse({"GET", "POST", "OPTIONS"});
        }
        else
        {
          theResponse = HTTP::Response::stockOptionsResponse({"GET", "OPTIONS"});
        }

        // Checking for CORS preflight headers
        // https://developer.mozilla.org/en-US/docs/Glossary/Preflight_request
        if (theRequest.getHeader("Access-Control-Request-Method"))
        {
          // Clone header 'Allow' to 'Access-Control-Allow-Methods' for CORS
          auto h1 = theResponse.getHeader("Allow");
          assert(bool(h1));  // HTTP::Response::stockOptionsResponse should have set this header
          theResponse.setHeader("Access-Control-Allow-Methods", *h1);

          auto opt_origin = theRequest.getHeader("Origin");
          if (opt_origin)
          {
            theResponse.setHeader("Access-Control-Allow-Origin", *opt_origin);
          }

          auto opt_req_headers = theRequest.getHeader("Access-Control-Request-Headers");
          if (opt_req_headers)
          {
            // FIXME: Should be use actaully supported headers here.
            //        Let us copy requested headers to the response for now
            theResponse.setHeader("Access-Control-Allow-Headers", *opt_req_headers);
          }

          theResponse.setHeader("Access-Control-Max-Age", "86400");
        }

        return true;

      default:
        throw Fmi::Exception(BCP, "Not supported request method " + theRequest.getMethodString());
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Plugin request check!");
  }
}
