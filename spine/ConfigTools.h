#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <macgyver/Exception.h>
#include <libconfig.h++>

namespace SmartMet
{
namespace Spine
{
bool lookupConfigSetting(const libconfig::Config& theConfig,
                         std::string& theValue,
                         const std::string& theVariable);

bool lookupPathSetting(const libconfig::Config& theConfig,
                       std::string& theValue,
                       const std::string& theVariable);

void expandVariables(libconfig::Config& theConfig);

std::string config_hash(const libconfig::Setting& setting);
std::string config_hash(const libconfig::Config& config);

// ----------------------------------------------------------------------
/*!
 * \brief Byte size settings
 *
 * A byte size may be given either as an integer or as a string with
 * an optional unit, so all of the following mean 32 gibibytes:
 *
 *     memory_bytes = 34359738368;
 *     memory_bytes = 34359738368L;
 *     memory_bytes = "34359738368";
 *     memory_bytes = "32G";
 *     memory_bytes = "32GB";
 *     memory_bytes = "32 GiB";
 *
 * The unit is case insensitive, and B, K, M, G, T and P are accepted
 * both alone and followed by "B" or "iB". All units are binary
 * multiples, so "1KB" and "1KiB" both mean 1024 bytes. Fractions such
 * as "1.5G" are rounded to the nearest byte.
 *
 * Note that libconfig requires an 'L' suffix for integers not fitting
 * into 32 bits, which is exactly why the string form is preferable for
 * large sizes.
 */
// ----------------------------------------------------------------------

std::size_t parseSize(const libconfig::Setting& theSetting);

// Return false if the setting does not exist. Host specific overrides are honoured.
bool lookupSizeSetting(const libconfig::Config& theConfig,
                       std::size_t& theValue,
                       const std::string& theVariable);

// Return the default value if the setting does not exist.
std::size_t lookupSizeSetting(const libconfig::Config& theConfig,
                              const std::string& theVariable,
                              std::size_t theDefault);

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
              for (int k = 0; k < value.getLength(); ++k)
              {
                theValue.emplace_back(value[k]);
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
              for (int k = 0; k < value.getLength(); ++k)
              {
                theValue.emplace_back(value[k].c_str());
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

}  // namespace Spine
}  // namespace SmartMet
