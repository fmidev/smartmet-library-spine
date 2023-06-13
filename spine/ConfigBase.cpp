#include "ConfigBase.h"
#include "ConfigTools.h"
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/spirit/include/qi.hpp>
#include <macgyver/Exception.h>
#include <cassert>
#include <iostream>
#include <vector>

namespace SmartMet
{
namespace Spine
{
ConfigBase::ConfigBase(const std::string& file_name, const std::string& config_name)
    : file_name(file_name), config_name(config_name), itsConfig(new libconfig::Config)
{
  try
  {
    if (file_name.empty())
    {
      throw Fmi::Exception(BCP, "Configuration not provided!")
          .addParameter("Name", config_name)
          .addParameter("Filename", file_name);
    }

    try
    {
      // Enable sensible relative include paths
      boost::filesystem::path p = file_name;
      p.remove_filename();
      itsConfig->setIncludeDir(p.c_str());

      itsConfig->readFile(file_name.c_str());
      expandVariables(*itsConfig);
    }
    catch (const libconfig::ConfigException&)
    {
      handle_libconfig_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

ConfigBase::ConfigBase(boost::shared_ptr<libconfig::Config> config, const std::string& config_name)
    : file_name("<none>"), config_name(config_name), itsConfig(config)
{
  try
  {
    if (not config)
    {
      throw Fmi::Exception(BCP, "Configuration not provided!")
          .addDetail("Empty boost::shared_ptr<>")
          .addParameter("Name", config_name);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

ConfigBase::~ConfigBase() = default;

void ConfigBase::handle_libconfig_exceptions(const std::string& location) const
{
  try
  {
    throw;
  }
  catch (const libconfig::SettingException& err)
  {
    throw Fmi::Exception(BCP, "Configuration file setting error!")
        .addParameter("Name", config_name)
        .addParameter("Exception type", Fmi::current_exception_type())
        .addParameter("Configuration file", file_name)
        .addParameter("Path", err.getPath())
        .addParameter("Location", location)
        .addParameter("Error description", err.what());
  }
  catch (libconfig::ParseException& err)
  {
    throw Fmi::Exception(BCP, "Configuration file parsing failed!")
        .addParameter("Name", config_name)
        .addParameter("Exception type", Fmi::current_exception_type())
        .addParameter("Configuration file", file_name)
        .addParameter("Error line", std::to_string(err.getLine()))
        .addParameter("Error description", err.getError());
  }
  catch (const libconfig::ConfigException& err)
  {
    throw Fmi::Exception(BCP, "Configuration exception!")
        .addParameter("Name", config_name)
        .addParameter("Exception type", Fmi::current_exception_type())
        .addParameter("Configuration file", file_name)
        .addParameter("Location", location)
        .addParameter("Error description", err.what());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }

  // Not supposed to be here
  // std::cerr << "INTERNAL ERROR in " << METHOD_NAME << ": not supposed to be here" << std::endl;
  // abort();
}

void ConfigBase::dump_config(std::ostream& stream, const libconfig::Config& config)
{
  try
  {
    libconfig::Setting& root = config.getRoot();
    if (root.isGroup())
    {
      int N = root.getLength();
      for (int i = 0; i < N; i++)
      {
        dump_setting(stream, root[i], 0);
        if (i < N + 1)
          stream << "\n";
      }
    }
    else
    {
      dump_setting(stream, root, 0);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void ConfigBase::dump_setting(std::ostream& stream,
                              const libconfig::Setting& setting,
                              std::size_t offset)
{
  try
  {
    const std::string prefix(offset, ' ');
    const char* name = setting.getName();

    int type = setting.getType();

    if (type == libconfig::Setting::TypeGroup)
    {
      stream << prefix;
      if (name)
        stream << name << ": ";
      stream << "{\n";
      int N = setting.getLength();
      for (int i = 0; i < N; i++)
      {
        dump_setting(stream, setting[i], offset + 4);
        if (i < N + 1)
          stream << "\n";
      }
      stream << prefix << "}";
    }
    else if (type == libconfig::Setting::TypeList)
    {
      stream << prefix;
      if (name)
        stream << name << ": ";
      stream << "(\n";
      int N = setting.getLength();
      for (int i = 0; i < N; i++)
      {
        if (i > 0)
          stream << ",\n";
        dump_setting(stream, setting[i], offset + 4);
      }
      stream << '\n' << prefix << ')';
    }
    else if (type == libconfig::Setting::TypeArray)
    {
      stream << prefix;
      if (name)
        stream << name << ": ";
      stream << "[\n";
      int N = setting.getLength();
      for (int i = 0; i < N; i++)
      {
        if (i > 0)
          stream << ",\n";
        dump_setting(stream, setting[i], offset + 4);
      }
      stream << '\n' << prefix << ']';
    }
    else if (type == libconfig::Setting::TypeInt)
    {
      int value = setting;
      stream << prefix;
      if (name)
        stream << name << " = ";
      stream << value;
    }
    else if (type == libconfig::Setting::TypeInt64)
    {
      long long value = setting;
      stream << prefix;
      if (name)
        stream << name << " = ";
      stream << value;
    }
    else if (type == libconfig::Setting::TypeFloat)
    {
      double value = setting;
      char buffer[80];
      snprintf(buffer, sizeof(buffer), "%.16g", value);
      stream << prefix;
      if (name)
        stream << name << " = ";
      stream << buffer;
    }
    else if (type == libconfig::Setting::TypeString)
    {
      // Conversion through const char * to avoid ambiguity
      const char* tmp = setting;
      const std::string value = tmp;
      stream << prefix;
      if (name)
        stream << name << " = ";
      stream << "\"" << value << "\"";
    }
    else if (type == libconfig::Setting::TypeBoolean)
    {
      const bool value = setting;
      stream << prefix;
      if (name)
        stream << name << " = ";
      stream << (value ? "true" : "false");
    }
    else
    {
      // Error: unknown type
      stream << prefix;
      if (name)
        stream << name << " = ";
      stream << '?';
      return;
    }

    if (name)
    {
      stream << ";";
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

libconfig::Setting& ConfigBase::assert_is_list(libconfig::Setting& setting, int min_size)
{
  try
  {
    if (not setting.isList())
    {
      std::ostringstream msg;
      const std::string path = setting.getPath();
      msg << "The length of the 'libconfig' list ";
      if (min_size > 0)
        msg << "expected to be at least '" << min_size << "' ";
      msg << "in the '" << format_path(nullptr, path) << "' path!";

      std::ostringstream msg2;
      dump_setting(msg2, setting);

      throw Fmi::Exception(BCP, msg.str())
          .addParameter("Configuration file", file_name)
          .addDetail(msg2.str());
    }

    if (setting.getLength() < min_size)
    {
      std::ostringstream msg;
      const std::string path = setting.getPath();
      msg << "The length '" << setting.getLength() << "' of "
          << "the 'libconfig' list in the '" << format_path(nullptr, path)
          << "' path is too small!";

      std::ostringstream msg2;
      dump_setting(msg2, setting);

      throw Fmi::Exception(BCP, msg.str())
          .addParameter("Configuration file", file_name)
          .addDetail(msg2.str());
    }

    return setting;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

libconfig::Setting& ConfigBase::assert_is_group(libconfig::Setting& setting)
{
  try
  {
    if (setting.isGroup())

      return setting;

    std::ostringstream msg;
    const std::string path = setting.getPath();
    msg << "The libconfig group expected in the '" << format_path(nullptr, path) << " path!";

    std::ostringstream msg2;
    dump_setting(msg2, setting);

    throw Fmi::Exception(BCP, msg.str()).addDetail(msg2.str());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

libconfig::Setting* ConfigBase::find_setting(libconfig::Setting& search_start,
                                             const std::string& path,
                                             bool mandatory) const
{
  return find_setting_impl(search_start, path, mandatory, 5);
}

libconfig::Setting* ConfigBase::find_setting_impl(libconfig::Setting& search_start,
                                                  const std::string& path,
                                                  bool mandatory,
                                                  int max_depth) const
{
  try
  {
    namespace ba = boost::algorithm;
    namespace qi = boost::spirit::qi;
    namespace ns = boost::spirit::standard;
    namespace bl = boost::lambda;

    libconfig::Setting* curr = &search_start;

    std::vector<std::string> parts;
    ba::split(parts, path, ba::is_any_of(".:/"), ba::token_compress_on);
    for (const auto& name : parts)
    {
      int ind = 0;

      if (curr->isScalar())
      {
        throw Fmi::Exception(BCP, "Incorrect path!")
            .addParameter("Configuration file", file_name)
            .addParameter("Path", format_path(&search_start, path))
            .addDetail(curr->getPath() + "' is scalar");
      }

      if (qi::phrase_parse(name.begin(),
                           name.end(),
                           qi::char_('[') >> qi::int_[bl::var(ind) = bl::_1] >> qi::char_(']'),
                           ns::space))
      {
        if (curr->isGroup())
        {
          throw Fmi::Exception(BCP, "Incorrect path!")
              .addParameter("Path", format_path(&search_start, path))
              .addDetail(curr->getPath() + "' is group when array or list is expected");
        }

        if (ind < 0 or ind >= curr->getLength())
        {
          if (!mandatory)
            return nullptr;

          throw Fmi::Exception(BCP, "Index in path is out of range!")
              .addParameter("Configuration file", file_name)
              .addParameter("Path", format_path(&search_start, path))
              .addParameter("Index", std::to_string(ind))
              .addParameter("Range", "0 .. " + std::to_string(curr->getLength() - 1));
        }

        curr = &(*curr)[ind];
      }
      else
      {
        if (curr->isGroup())
        {
          if (curr->exists(name))
          {
            curr = &(*curr)[name.c_str()];
          }
          else if (mandatory)
          {
            throw Fmi::Exception(BCP, "Path not found!")
                .addParameter("Configuration file", file_name)
                .addParameter("Path", format_path(curr, name));
          }
          else
          {
            return nullptr;
          }
        }
        else
        {
          throw Fmi::Exception(BCP, "Incorrect path!")
              .addParameter("Configuration file", file_name)
              .addParameter("Path", format_path(curr, name))
              .addDetail(curr->getPath() + "' is not a group");
        }
      }
    }

    if (curr and curr->isScalar() and curr->getType() == libconfig::Setting::TypeString)
    {
      const char* content = *curr;
      if (strncasecmp(content, "%[", 2) == 0)
      {
        const char* end = std::strrchr(content + 2, ']');
        if (end and strcmp(end, "]") == 0)
        {
          if (max_depth >= 0)
          {
            std::string redirect(content + 2, end);
            curr = find_setting_impl(itsConfig->getRoot(), redirect, mandatory, max_depth - 1);
          }
          else
          {
            throw Fmi::Exception(BCP, "Configuration parameter redirection depth exceeded")
                .addParameter("Configuration file", file_name)
                .addParameter("Path", format_path(curr, config_name));
          }
        }
      }
    }

    return curr;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string ConfigBase::format_path(libconfig::Setting* origin, const std::string& path) const
{
  try
  {
    std::ostringstream result;
    const std::string origin_path = origin ? origin->getPath() : std::string("");
    result << '{' << get_file_name() << "}:";
    if (origin_path != "")
      result << origin_path << '.';
    result << path;
    return result.str();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

template <>
std::string ConfigBase::get_setting_value<std::string>(const libconfig::Setting& setting)
{
  try
  {
    const char* result = setting;
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

template <>
libconfig::Setting& ConfigBase::get_mandatory_config_param(libconfig::Setting& setting,
                                                           const std::string& path)
{
  try
  {
    try
    {
      libconfig::Setting& item = *find_setting(setting, path, true);
      return item;
    }
    catch (const libconfig::ConfigException&)
    {
      handle_libconfig_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

template <>
std::size_t ConfigBase::get_setting_value<std::size_t>(const libconfig::Setting& setting)
{
  try
  {
    // libconfig API does not support std::size_t explicitly, hence fetch via int
    return boost::numeric_cast<std::size_t>(static_cast<int>(setting));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string ConfigBase::get_optional_path(const std::string& theName,
                                          const std::string& theDefault) const
{
  if (!itsConfig)
    throw Fmi::Exception(BCP, "Config missing");

  std::string value;
  if (!itsConfig->lookupValue(theName, value))
    value = theDefault;

  // Allow empty values too
  if (value.empty())
    return value;

  if (value[0] != '/')
  {
    boost::filesystem::path p(file_name);
    auto prefix = p.parent_path().string();
    // prevent "f.conf" from being expanded to "/f.conf"
    if (!prefix.empty())
      value = prefix + "/" + value;
  }
  return value;
}

std::string ConfigBase::get_mandatory_path(const std::string& theName) const
{
  if (!itsConfig)
    throw Fmi::Exception(BCP, "Config missing");

  std::string value;
  if (!itsConfig->lookupValue(theName, value))
    throw Fmi::Exception(BCP, "Configuration variable " + theName + " is mandatory");

  if (value[0] != '/')
  {
    boost::filesystem::path p(file_name);
    auto prefix = p.parent_path().string();
    // prevent "f.conf" from being expanded to "/f.conf"
    if (!prefix.empty())
      value = prefix + "/" + value;
  }

  return value;
}

std::vector<std::string> ConfigBase::get_mandatory_path_array(const std::string& theName,
                                                              int min_size,
                                                              int max_size)
{
  try
  {
    if (!itsConfig)
      throw Fmi::Exception(BCP, "Config missing");

    std::vector<std::string> paths;
    if (!get_config_array(theName, paths, min_size, max_size))
      throw Fmi::Exception(BCP, "Failed to read array of strings from variable " + theName);

    boost::filesystem::path p(file_name);

    for (auto& path : paths)
    {
      if (!path.empty() && path[0] != '/')
        path = p.parent_path().string() + "/" + path;
    }
    return paths;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!")
        .addParameter("Path array variable name", theName);
  }
}

}  // namespace Spine
}  // namespace SmartMet
