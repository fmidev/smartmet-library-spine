#include "ConfigTools.h"
#include <boost/filesystem.hpp>

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

  if(lookupHostSetting(theConfig, theValue, configvar))
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

  std::string hostname = boost::asio::ip::host_name();
  while (!hostname.empty())
  {
    std::string tmp = configpath + "/" + hostname + ".conf";
    if (boost::filesystem::exists(tmp))
    {
      theValue = tmp;
      return true;
    }

    // Remove next .suffix from hostname

    auto lastpos = hostname.find_last_of('.');
    if (lastpos == std::string::npos)
      return false;

    hostname = hostname.substr(0, lastpos);
  }

  return false;
}

}  // namespace Spine
}  // namespace SmartMet
