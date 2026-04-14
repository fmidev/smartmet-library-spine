#include "ConfigTools.h"
#include <filesystem>
#include <macgyver/Base64.h>
#include <macgyver/Exception.h>
#include <openssl/sha.h>
#include <cstdlib>

namespace
{

void sha256_update(SHA256_CTX& ctx, const void* data, std::size_t len)
{
  SHA256_Update(&ctx, data, len);
}

void sha256_update(SHA256_CTX& ctx, const std::string& str)
{
  sha256_update(ctx, str.data(), str.size());
}

template <typename T>
void sha256_update_value(SHA256_CTX& ctx, const T& value)
{
  sha256_update(ctx, &value, sizeof(value));
}

void sha256_update_setting(SHA256_CTX& ctx, const libconfig::Setting& setting)
{
  const std::string path = setting.getPath();
  sha256_update(ctx, path);

  const int type = setting.getType();
  sha256_update_value(ctx, type);

  switch (type)
  {
    case libconfig::Setting::TypeInt:
    {
      const int value = setting;
      sha256_update_value(ctx, value);
      break;
    }
    case libconfig::Setting::TypeInt64:
    {
      const long long value = setting;
      sha256_update_value(ctx, value);
      break;
    }
    case libconfig::Setting::TypeFloat:
    {
      const double value = setting;
      sha256_update_value(ctx, value);
      break;
    }
    case libconfig::Setting::TypeString:
    {
      const char* value = setting;
      sha256_update(ctx, value, std::strlen(value));
      break;
    }
    case libconfig::Setting::TypeBoolean:
    {
      const bool value = setting;
      sha256_update_value(ctx, value);
      break;
    }
    case libconfig::Setting::TypeGroup:
    case libconfig::Setting::TypeList:
    case libconfig::Setting::TypeArray:
    {
      const int len = setting.getLength();
      sha256_update_value(ctx, len);
      for (int i = 0; i < len; i++)
        sha256_update_setting(ctx, setting[i]);
      break;
    }
    default:
      break;
  }
}

}  // anonymous namespace

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

  std::filesystem::path p{theConfig.lookup(theVariable).getSourceFile()};
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

  if (!std::filesystem::is_directory(configpath))
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
    if (std::filesystem::exists(tmp))
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
            std::string varname = (theSetting.getName() != nullptr ? theSetting.getName() : "");
            if (var != varname && theConfig.exists(var))
            {
              const auto& setting = theConfig.lookup(var);
              if (setting.getType() != libconfig::Setting::TypeString)
                throw Fmi::Exception(BCP, "Value for setting " + varValue + " must be a string");
              varValue = setting.c_str();
            }
            else
            {
              // Variable not defined in the configuration file. Maybe it is an environment variable

              // This is too secure for developers wanting for example access to $HOME to use their
              // own configuration files. Static analyzers may complain about this.
              // char* env = secure_getenv(var.c_str());

              char* env = getenv(var.c_str());
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

// ----------------------------------------------------------------------
/*!
 * \brief Compute SHA256 hash of a libconfig::Setting tree
 *
 * Recursively hashes the path, type, and value of each setting,
 * producing a content-based digest that is independent of file
 * modification time and whitespace.  The result is base64 encoded.
 */
// ----------------------------------------------------------------------

std::string config_hash(const libconfig::Setting& setting)
{
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  sha256_update_setting(ctx, setting);
  unsigned char digest[SHA256_DIGEST_LENGTH];
  SHA256_Final(digest, &ctx);
  return Fmi::Base64::encode(std::string(reinterpret_cast<const char*>(digest), SHA256_DIGEST_LENGTH));
}

// ----------------------------------------------------------------------
/*!
 * \brief Compute SHA256 hash of a libconfig::Config (its root setting)
 */
// ----------------------------------------------------------------------

std::string config_hash(const libconfig::Config& config)
{
  return config_hash(config.getRoot());
}

}  // namespace Spine
}  // namespace SmartMet
