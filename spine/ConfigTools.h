#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <macgyver/Exception.h>
#include <libconfig.h++>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Return a setting, which may have a host specific value
 *
 * Example:
 *
 *   verbose = false;
 *
 *   overrides:
 *   (
 *       {
 *           name = ["host1","host2"];
 *           verbose = true;
 *       };
 *       ...
 *   )
 */
// ----------------------------------------------------------------------

template <typename T>
bool lookupHostSetting(const libconfig::Config& theConfig,
                       T& theValue,
                       const std::string& theVariable)
{
  const std::string hostname = boost::asio::ip::host_name();

  try
  {
    // scan for overrides
    if (theConfig.exists("overrides"))
    {
      const libconfig::Setting& override = theConfig.lookup("overrides");
      int count = override.getLength();
      for (int i = 0; i < count; ++i)
      {
        const libconfig::Setting& trial_hosts = override[i]["name"];
        int numhosts = trial_hosts.getLength();
        for (int j = 0; j < numhosts; ++j)
        {
          std::string trial_host = trial_hosts[j];
          // Does the start of the host name match and there is a value for the setting?
          if (boost::algorithm::istarts_with(hostname, trial_host))
          {
            std::string path = "overrides.[" + std::to_string(i) + "]." + theVariable;
            if (theConfig.lookupValue(path, theValue))
              return true;
          }
        }
      }
    }

    // use default setting instead
    return theConfig.lookupValue(theVariable, theValue);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Error trying to find setting value")
        .addParameter("variable", theVariable);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return a setting of form T<element>, which may have a host specific value
 *
 * The container may be a list or a vector. The element cannot be std::string
 * due to operator overloading ambiguities.
 */
// ----------------------------------------------------------------------

template <typename T>
bool lookupHostSettings(const libconfig::Config& theConfig,
                        T& theValue,
                        const std::string& theVariable)
{
  const std::string hostname = boost::asio::ip::host_name();

  try
  {
    // scan for overrides
    if (theConfig.exists("overrides"))
    {
      const libconfig::Setting& override = theConfig.lookup("overrides");
      int count = override.getLength();
      for (int i = 0; i < count; ++i)
      {
        const libconfig::Setting& trial_hosts = override[i]["name"];
        int numhosts = trial_hosts.getLength();
        for (int j = 0; j < numhosts; ++j)
        {
          std::string trial_host = trial_hosts[j];
          // Does the start of the host name match and there is a value for the setting?
          if (boost::algorithm::istarts_with(hostname, trial_host))
          {
            std::string path = "overrides.[" + std::to_string(i) + "]." + theVariable;
            if (theConfig.exists(path))
            {
              const auto& value = theConfig.lookup(path);
              for (int i = 0; i < value.getLength(); ++i)
              {
                theValue.emplace_back(value[i]);
              }
              return true;
            }
          }
        }
      }
    }

    // use default setting instead
    if (theConfig.exists(theVariable))
    {
      const auto& value = theConfig.lookup(theVariable);
      for (int i = 0; i < value.getLength(); ++i)
      {
        theValue.emplace_back(value[i]);
      }
      return true;
    }

    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Error trying to find setting value")
        .addParameter("variable", theVariable);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return a setting of form T<std::string>, which may have a host specific value
 */
// ----------------------------------------------------------------------

template <typename T>
bool lookupHostStringSettings(const libconfig::Config& theConfig,
                              T& theValue,
                              const std::string& theVariable)
{
  const std::string hostname = boost::asio::ip::host_name();

  try
  {
    // scan for overrides
    if (theConfig.exists("overrides"))
    {
      const libconfig::Setting& override = theConfig.lookup("overrides");
      int count = override.getLength();
      for (int i = 0; i < count; ++i)
      {
        const libconfig::Setting& trial_hosts = override[i]["name"];
        int numhosts = trial_hosts.getLength();
        for (int j = 0; j < numhosts; ++j)
        {
          std::string trial_host = trial_hosts[j];
          // Does the start of the host name match and there is a value for the setting?
          if (boost::algorithm::istarts_with(hostname, trial_host))
          {
            std::string path = "overrides.[" + std::to_string(i) + "]." + theVariable;
            if (theConfig.exists(path))
            {
              const auto& value = theConfig.lookup(path);
              for (int i = 0; i < value.getLength(); ++i)
              {
                theValue.emplace_back(value[i].c_str());
              }
              return true;
            }
          }
        }
      }
    }

    // use default setting instead
    if (theConfig.exists(theVariable))
    {
      const auto& value = theConfig.lookup(theVariable);
      for (int i = 0; i < value.getLength(); ++i)
      {
        theValue.emplace_back(value[i].c_str());
      }
      return true;
    }

    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Error trying to find setting value")
        .addParameter("variable", theVariable);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Lookup a config setting (.configfile or .configpath)
 */
// ----------------------------------------------------------------------

bool lookupConfigSetting(const libconfig::Config& theConfig,
                         std::string& theValue,
                         const std::string& theVariable);

// ----------------------------------------------------------------------
/*!
 * \brief Lookup a path setting
 */
// ----------------------------------------------------------------------

bool lookupPathSetting(const libconfig::Config& theConfig,
                       std::string& theValue,
                       const std::string& theVariable);

}  // namespace Spine
}  // namespace SmartMet
