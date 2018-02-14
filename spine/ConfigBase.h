#pragma once

#include "Exception.h"
#include <boost/shared_ptr.hpp>
#include <macgyver/TypeName.h>
#include <libconfig.h++>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Spine
{
/**
 *   @brief Base class for libconfig format configuration
 *
 *   Main reason for this base class is to
 *   - provide useful methods for retrieving configuration settings
 *   - provide method for libconfig++ exceptions handling
 */
class ConfigBase
{
 public:
  /**
   *   @brief Constructor for ConfigBase object
   *
   *   @param file_name The name of the configuration file to parse
   *   @param name The name of the configuration for error messages (only
   *          used if non empty string specified)
   */
  ConfigBase(const std::string& file_name, const std::string& name = "");

  /**
   *   @brief Constructor for ConfigBase object
   *
   *   @param config The actual config object yto use.
   *   @param name The name of the configuration for error messages (only
   *          used if non empty string specified)
   */
  ConfigBase(boost::shared_ptr<libconfig::Config> config, const std::string& name = "");

  virtual ~ConfigBase();

  inline const std::string& get_file_name() const { return file_name; }
  inline const std::string& get_name() const { return name; }
  inline libconfig::Config& get_config() { return *itsConfig; }
  inline libconfig::Setting& get_root() { return itsConfig->getRoot(); }
  template <typename Type>
  inline bool get_config_param(const std::string& theName, Type* result)
  {
    return get_config_param(itsConfig->getRoot(), theName, result);
  }

  template <typename Type>
  bool get_config_param(libconfig::Setting& setting, const std::string& theName, Type* result)
  {
    try
    {
      if (setting.exists(name))
      {
        *result = get_mandatory_config_param<Type>(setting, theName);
        return true;
      }
      else
      {
        return false;
      }
    }
    catch (...)
    {
      throw Exception::Trace(BCP, "Operation failed!").addParameter("Parameter", theName);
    }
  }

  /**
   *   @brief Gets mandatory configuration parameter by its path relative to configuration file root
   *
   *   @param theName the path to the element (for example "foo" or "foo.bar" or "foo[3].tar")
   *
   *   The type of configuration value must be specified as a template parameter.
   *   - basic numeric types (like @b int or @b double) are supported
   *   - std::string is supported
   *   - <b>libconfig::Setting&</b> may also be specified as template argument. In this case
   *     reference to found libconfig::Setting& object is returned
   *   Other template parameter types (except these mentioned above) are not supported.
   */
  template <typename Type>
  inline Type get_mandatory_config_param(const std::string& theName)
  {
    try
    {
      return get_mandatory_config_param<Type>(itsConfig->getRoot(), theName);
    }
    catch (...)
    {
      throw Exception::Trace(BCP, "Operation failed!").addParameter("Parameter", theName);
    }
  }

  /**
   *   @brief Gets mandatory configuration parameter by its path relative to specified
   * libconfig\::Setting object
   *
   *   @param setting configuration setting from which to look for parameter
   *   @param theName the path to the element (for example "foo" or "foo.bar" or "foo[3].tar")
   *
   *   The type of configuration value must be specified as a template parameter.
   *   - basic numeric types (like @b int or @b double) are supported
   *   - std::string is supported
   *   - <b>libconfig::Setting&</b> may also be specified as template argument. In this case
   *     reference to found libconfig::Setting& object is returned
   *   Other template parameter types (except these mentioned above) are not supported.
   */
  template <typename Type>
  Type get_mandatory_config_param(libconfig::Setting& setting, const std::string& theName)
  {
    try
    {
      Type result;

      try
      {
        const libconfig::Setting& item = *find_setting(setting, theName, true);
        result = get_setting_value<Type>(item);
      }
      catch (const libconfig::ConfigException&)
      {
        handle_libconfig_exceptions(METHOD_NAME);
      }

      return result;
    }
    catch (...)
    {
      throw Exception::Trace(BCP, "Operation failed!").addParameter("Parameter", theName);
    }
  }

  /**
   *   @brief Gets optional configuration parameter by its path relative to configuration file root
   *
   *   Provided default value is returned if configuration setting is not found.
   *
   *   @param theName the path to the element (for example "foo" or "foo.bar" or "foo[3].tar")
   *
   *   The type of configuration value must be specified as a template parameter.
   *   - basic numeric types (like @b int or @b double) are supported
   *   - std::string is also supported
   *   Other template parameter types (except these mentioned above) are not supported.
   */
  template <typename Type>
  inline Type get_optional_config_param(const std::string& theName, Type default_value)
  {
    try
    {
      return get_optional_config_param<Type>(itsConfig->getRoot(), theName, default_value);
    }
    catch (...)
    {
      throw Exception::Trace(BCP, "Operation failed!").addParameter("Parameter", theName);
    }
  }

  /**
   *   @brief Gets optional configuration parameter by its path relative to specified
   * libconfig\::Setting.
   *
   *   Provided default value is returned if configuration setting is not found.
   *
   *   @param path the path to the element (for example "foo" or "foo.bar" or "foo[3].tar")
   *
   *   The type of configuration value must be specified as a template parameter.
   *   - basic numeric types (like @b int or @b double) are supported
   *   - std::string is also supported
   *   Other template parameter types (except these mentioned above) are not supported.
   */
  template <typename Type>
  Type get_optional_config_param(libconfig::Setting& setting,
                                 const std::string& path,
                                 Type default_value)
  {
    try
    {
      Type result = default_value;

      libconfig::Setting* item = find_setting(setting, path, false);
      if (item)
      {
        try
        {
          result = get_setting_value<Type>(*item);
        }
        catch (const libconfig::ConfigException&)
        {
          handle_libconfig_exceptions(METHOD_NAME);
        }
      }
      return result;
    }
    catch (...)
    {
      throw Exception::Trace(BCP, "Operation failed!").addParameter("Path", path);
    }
  }

  /**
   *   @brief Reads an array from configuration file (path is provided relative to specified
   * cnfiguration root.
   *
   *   Providing scalar instead of an array is supported. In this
   *   case the  returned vector will contain 1 element.
   *
   *   @param path path of array value relative to libconfig::Setting provided in the first
   * parameter
   *   @param result vector where to store the result (vector is emptied at the start)
   *   @param min_size lower allowed size of the array (default 0)
   *   @param max_size upper allowed size of the array
   *   @retval true Array value is found and is sucessfully stored into provided std::vector
   *   @retval false Array value is not found
   *
   *   std::runtime_error is being thrown if the value is found at provided path but its format is
   *   incorrect.
   *
   *   The supported types are the same as for ConfigBase::get_mandatory_config_param
   *   except than <b>libconfig::Setting&</b> is not supported.
   */
  template <typename MemberType>
  inline bool get_config_array(const std::string& path,
                               std::vector<MemberType>& result,
                               int min_size = 0,
                               int max_size = std::numeric_limits<int>::max())
  {
    try
    {
      return get_config_array<MemberType>(itsConfig->getRoot(), path, result, min_size, max_size);
    }
    catch (...)
    {
      throw Exception::Trace(BCP, "Operation failed!").addParameter("Path", path);
    }
  }

  /**
   *   @brief Reads an array from configuration file (path is provided relative to specified
   * libconfig\::Setting)
   *
   *   Providing scalar instead of an array is supported. In this
   *   case the  returned vector will contain 1 element.
   *
   *   @param parent libconfig\::Setting from which to search the array value
   *   @param path path of array value relative to libconfig::Setting provided in the first
   * parameter
   *   @param result vector where to store the result (vector is emptied at the start)
   *   @param min_size lower allowed size of the array (default 0)
   *   @param max_size upper allowed size of the array
   *   @retval true Array value is found and is sucessfully stored into provided std::vector
   *   @retval false Array value is not found
   *
   *   std::runtime_error is being thrown if the value is found at provided path but its format is
   *   incorrect.
   *
   *   The supported types are the same as for ConfigBase::get_mandatory_config_param
   *   except than <b>libconfig::Setting&</b> is not supported.
   */
  template <typename MemberType>
  bool get_config_array(libconfig::Setting& parent,
                        const std::string& path,
                        std::vector<MemberType>& result,
                        int min_size = 0,
                        int max_size = std::numeric_limits<int>::max())
  {
    try
    {
      try
      {
        result.clear();
        libconfig::Setting* tmp1 = find_setting(parent, path, false);
        if (tmp1)
        {
          libconfig::Setting& setting = *tmp1;

          if (setting.isArray())
          {
            int len = setting.getLength();
            if (len < min_size or len > max_size)
            {
              throw Exception(BCP, "The size of the array setting is out of the range!")
                  .addParameter("Configuration file", file_name)
                  .addParameter("Size", std::to_string(len))
                  .addParameter("Range",
                                std::to_string(min_size) + " .. " + std::to_string(max_size));
            }
            for (int i = 0; i < len; i++)
            {
              MemberType member = get_setting_value<MemberType>(setting[i]);
              result.push_back(member);
            }
            return true;
          }
          else if (setting.isScalar())
          {
            if (min_size > 1 or max_size < 1)
            {
              throw Exception(BCP, "The size of the array setting is out of the range!")
                  .addParameter("Configuration file", file_name)
                  .addParameter("Size", "1")
                  .addParameter("Range",
                                std::to_string(min_size) + " .. " + std::to_string(max_size));
            }
            MemberType member = get_setting_value<MemberType>(setting);
            result.push_back(member);
            return true;
          }
          else
          {
            throw Exception(BCP, "Incorrect configuration type!")
                .addParameter("Configuration file", file_name)
                .addParameter("Path", path)
                .addDetail("Scalar or array of " +
                           Fmi::demangle_cpp_type_name(typeid(MemberType).name()) +
                           "was expected!");
          }
        }
        else
        {
          return false;
        }
      }
      catch (const libconfig::ConfigException&)
      {
        handle_libconfig_exceptions(METHOD_NAME);
      }
    }
    catch (...)
    {
      throw Exception::Trace(BCP, "Operation failed!").addParameter("Path", path);
    }
  }

  template <typename MemberType>
  inline std::vector<MemberType> get_mandatory_config_array(
      const std::string& path, int min_size = 0, int max_size = std::numeric_limits<int>::max())
  {
    try
    {
      return get_mandatory_config_array<MemberType>(itsConfig->getRoot(), path, min_size, max_size);
    }
    catch (...)
    {
      throw Exception::Trace(BCP, "Operation failed!").addParameter("Path", path);
    }
  }

  template <typename MemberType>
  std::vector<MemberType> get_mandatory_config_array(libconfig::Setting& setting,
                                                     const std::string& path,
                                                     int min_size = 0,
                                                     int max_size = std::numeric_limits<int>::max())
  {
    try
    {
      std::vector<MemberType> result;
      if (not this->get_config_array(setting, path, result, min_size, max_size))
      {
        throw Exception(BCP, "Mandatory array setting not found!")
            .addParameter("Configuration file", file_name)
            .addParameter("Path", path);
      }
      return result;
    }
    catch (...)
    {
      throw Exception::Trace(BCP, "Operation failed!").addParameter("Path", path);
    }
  }

  libconfig::Setting* find_setting(libconfig::Setting& search_start,
                                   const std::string& path,
                                   bool mandatory) const;

  /**
   *   @brief Translates libconfig++ exceptions to std::runtime_error and throw it
   */
  void handle_libconfig_exceptions(const std::string& location) const __attribute__((noreturn));

  static void dump_config(std::ostream& stream, const libconfig::Config& config);

  static void dump_setting(std::ostream& stream,
                           const libconfig::Setting& setting,
                           std::size_t offset = 0);

  libconfig::Setting& assert_is_list(libconfig::Setting& setting, int min_size = 0);
  libconfig::Setting& assert_is_group(libconfig::Setting& setting);

  std::string format_path(libconfig::Setting* origin, const std::string& path) const;

  std::string get_optional_path(const std::string& theName, const std::string& theDefault) const;
  std::string get_mandatory_path(const std::string& theName) const;

  std::vector<std::string> get_mandatory_path_array(const std::string& theName,
                                                    int min_size = 0,
                                                    int max_size = std::numeric_limits<int>::max());

 private:
  /**
   *   @brief Add method to workaround ambiguity of assignment of libconfig::Setting to std::string
   *          by providing partial specialization of this template method (see below)
   */
  template <typename Type>
  static Type get_setting_value(const libconfig::Setting& setting)
  {
    return static_cast<Type>(setting);
  }

 private:
  const std::string file_name;
  const std::string name;
  boost::shared_ptr<libconfig::Config> itsConfig;
};

// Partial specialization of template method for workarounding assignment ambiguity problem
template <>
libconfig::Setting& ConfigBase::get_mandatory_config_param(libconfig::Setting& setting,
                                                           const std::string& name);

template <>
std::string ConfigBase::get_setting_value<std::string>(const libconfig::Setting& setting);

template <>
std::size_t ConfigBase::get_setting_value<std::size_t>(const libconfig::Setting& setting);

}  // namespace Spine
}  // namespace SmartMet
