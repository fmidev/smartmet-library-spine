#include "ConfigTools.h"
#include <boost/filesystem.hpp>
#include <macgyver/Exception.h>
#include <cstdlib>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Fix a path value setting
 */
// ----------------------------------------------------------------------

bool fixPathSetting(const libconfig::Config& theConfig,
                    std::string& theValue,
                    const std::string& theVariable)
{
  if (theValue.empty())
    return false;

  if (theValue[0] == '/')
    return true;

  boost::filesystem::path p{theConfig.lookup(theVariable).getSourceFile()};
  auto prefix = p.parent_path().string();

  // prevent "f.conf" from being expanded to "/f.conf"
  if (!prefix.empty())
    theValue = prefix + "/" + theValue;

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Lookup a path setting
 */
// ----------------------------------------------------------------------

bool lookupPathSetting(const libconfig::Config& theConfig,
                       std::string& theValue,
                       const std::string& theVariable)
{
  if (!theConfig.lookupValue(theVariable, theValue))
    return false;
  return fixPathSetting(theConfig, theValue, theVariable);
}

// ----------------------------------------------------------------------
/*!
 * \brief Lookup a config path setting
 */
// ----------------------------------------------------------------------

bool lookupConfigSetting(const libconfig::Config& theConfig,
                         std::string& theValue,
                         const std::string& theVariable)
{
  std::string configvar = theVariable + ".configfile";

  if (lookupHostSetting(theConfig, theValue, configvar))
    return fixPathSetting(theConfig, theValue, configvar);

  // Then try finding variable.configpath and then host specific
  // configuration files from there

  configvar = theVariable + ".configpath";

  std::string configpath;
  if (!lookupPathSetting(theConfig, configpath, configvar))
    return false;

  // Try searching the directory for host specific files

  if (!boost::filesystem::is_directory(configpath))
    return false;

  // For name.host.domain try in this order:
  //   name.host.domain.conf
  //   name.host.conf
  //   name.conf

  std::string failed_filenames;

  std::string hostname = boost::asio::ip::host_name();
  while (!hostname.empty())
  {
    std::string tmp = configpath + "/" + hostname + ".conf";
    if (boost::filesystem::exists(tmp))
    {
      theValue = tmp;
      return true;
    }

    if (!failed_filenames.empty())
      failed_filenames += ", ";
    failed_filenames += tmp;

    // Remove next .suffix from hostname

    auto lastpos = hostname.find_last_of('.');
    if (lastpos == std::string::npos)
      break;

    hostname.resize(lastpos);
  }

  // If configpath is used, there really should be a host specific
  // configuration file. Hence we do not just return false, but
  // throw instead in order to provide a good error message.

  throw Fmi::Exception(BCP, "Host specific configuration file not found")
      .addParameter("component", theVariable)
      .addParameter("configpath", configpath)
      .addParameter("hostname", boost::asio::ip::host_name())
      .addDetail("Tested filenames: " + failed_filenames);
}

// ----------------------------------------------------------------------
/*!
 * \brief Expand internal and environment variables in string settings
 */
// ----------------------------------------------------------------------

void expandVariables(libconfig::Config& theConfig, libconfig::Setting& theSetting)
{
  switch (theSetting.getType())
  {
    case libconfig::Setting::TypeString:
    {
      // Copypast from grid-files ConfigurationFile::parseValue
      std::string val = theSetting;

      std::size_t p1 = 0;
      while (p1 != std::string::npos)
      {
        p1 = val.find("$(");
        if (p1 != std::string::npos)
        {
          std::size_t p2 = val.find(')', p1 + 1);
          if (p2 != std::string::npos)
          {
            std::string var = val.substr(p1 + 2, p2 - p1 - 2);
            std::string varValue;

            // Searching a value for the variable but do not allow USER=$(USER)
            if (var != theSetting.getName() && theConfig.exists(var))
            {
              const auto& setting = theConfig.lookup(var);
              if (setting.getType() != libconfig::Setting::TypeString)
                throw Fmi::Exception(BCP, "Value for setting " + varValue + " must be a string");
              varValue = setting.c_str();
            }
            else
            {
              // Variable not defined in the configuration file. Maybe it is an environment variable
              char* env = secure_getenv(var.c_str());
              if (env == nullptr)
              {
                throw Fmi::Exception(BCP, "Unknown variable name!")
                    .addParameter("VariableName", var);
              }

              varValue = env;
            }

            std::string newVal = val.substr(0, p1) + varValue + val.substr(p2 + 1);
            val = newVal;
          }
          else
          {
            throw Fmi::Exception(BCP,
                                 "Expecting the character ')' at the end of the variable name!")
                .addParameter("Value", val);
          }
        }
      }
      theSetting = val;  // Replace original value with expanded value

      break;
    }
    case libconfig::Setting::TypeGroup:
    case libconfig::Setting::TypeArray:
    case libconfig::Setting::TypeList:
    {
      for (auto i = 0; i < theSetting.getLength(); i++)
        expandVariables(theConfig, theSetting[i]);
      break;
    }
    default:
      break;
  }
}

void expandVariables(libconfig::Config& theConfig)
{
  expandVariables(theConfig, theConfig.getRoot());
}

}  // namespace Spine
}  // namespace SmartMet
