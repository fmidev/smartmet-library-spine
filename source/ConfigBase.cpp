#include "ConfigBase.h"
#include "Exception.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/numeric/conversion/cast.hpp>

namespace SmartMet
{
namespace Spine
{
ConfigBase::ConfigBase(const std::string& file_name, const std::string& name)
    : file_name(file_name), name(name), itsConfig(new libconfig::Config)
{
  try
  {
    if (file_name.empty())
    {
      SmartMet::Spine::Exception exception(BCP, "Configuration not provided!");
      if (name != "")
        exception.addParameter("Name", name);
      throw exception;
    }

    try
    {
      itsConfig->readFile(file_name.c_str());
    }
    catch (const libconfig::ConfigException&)
    {
      handle_libconfig_exceptions(METHOD_NAME);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

ConfigBase::ConfigBase(boost::shared_ptr<libconfig::Config> config, const std::string& name)
    : file_name("<none>"), name(name), itsConfig(config)
{
  try
  {
    if (not config)
    {
      SmartMet::Spine::Exception exception(BCP, "Configuration not provided!");
      exception.addDetail("Empty boost::shared_ptr<>");
      if (name != "")
        exception.addParameter("Name", name);
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

ConfigBase::~ConfigBase()
{
}

void ConfigBase::handle_libconfig_exceptions(const std::string& location) const
{
  try
  {
    throw;
  }
  catch (const libconfig::SettingException& err)
  {
    SmartMet::Spine::Exception exception(BCP, "Configuration file setting error!");
    exception.addParameter("Name", name);
    exception.addParameter("Exception type", Fmi::current_exception_type());
    exception.addParameter("Configuration file", file_name);
    exception.addParameter("Path", err.getPath());
    exception.addParameter("Location", location);
    exception.addParameter("Error description", err.what());
    throw exception;
  }
  catch (libconfig::ParseException& err)
  {
    SmartMet::Spine::Exception exception(BCP, "Configuration file parsing failed!");
    exception.addParameter("Name", name);
    exception.addParameter("Exception type", Fmi::current_exception_type());
    exception.addParameter("Configuration file", file_name);
    exception.addParameter("Error line", std::to_string(err.getLine()));
    exception.addParameter("Error description", err.getError());
    throw exception;
  }
  catch (const libconfig::ConfigException& err)
  {
    SmartMet::Spine::Exception exception(BCP, "Configuration exception!");
    exception.addParameter("Name", name);
    exception.addParameter("Exception type", Fmi::current_exception_type());
    exception.addParameter("Configuration file", file_name);
    exception.addParameter("Location", location);
    exception.addParameter("Error description", err.what());
    throw exception;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
      msg << "in the '" << format_path(NULL, path) << "' path!";

      std::ostringstream msg2;
      dump_setting(msg2, setting);

      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter("Configuration file", file_name);
      exception.addDetail(msg2.str());
      throw exception;
    }

    if (setting.getLength() < min_size)
    {
      std::ostringstream msg;
      const std::string path = setting.getPath();
      msg << "The length '" << setting.getLength() << "' of "
          << "the 'libconfig' list in the '" << format_path(NULL, path) << "' path is too small!";

      std::ostringstream msg2;
      dump_setting(msg2, setting);

      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addParameter("Configuration file", file_name);
      exception.addDetail(msg2.str());
      throw exception;
    }

    return setting;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

libconfig::Setting& ConfigBase::assert_is_group(libconfig::Setting& setting)
{
  try
  {
    if (setting.isGroup())
    {
      return setting;
    }
    else
    {
      std::ostringstream msg;
      const std::string path = setting.getPath();
      msg << "The libconfig group expected in the '" << format_path(NULL, path) << " path!";

      std::ostringstream msg2;
      dump_setting(msg2, setting);

      SmartMet::Spine::Exception exception(BCP, msg.str());
      exception.addDetail(msg2.str());
      throw exception;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

libconfig::Setting* ConfigBase::find_setting(libconfig::Setting& search_start,
                                             const std::string& path,
                                             bool mandatory) const
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
    BOOST_FOREACH (const auto& name, parts)
    {
      int ind = 0;

      if (curr->isScalar())
      {
        SmartMet::Spine::Exception exception(BCP, "Incorrect path!");
        exception.addParameter("Configuration file", file_name);
        exception.addParameter("Path", format_path(&search_start, path));
        exception.addDetail(curr->getPath() + "' is scalar");
        throw exception;
      }

      if (qi::phrase_parse(name.begin(),
                           name.end(),
                           qi::char_('[') >> qi::int_[bl::var(ind) = bl::_1] >> qi::char_(']'),
                           ns::space))
      {
        if (curr->isGroup())
        {
          SmartMet::Spine::Exception exception(BCP, "Incorrect path!");
          exception.addParameter("Path", format_path(&search_start, path));
          exception.addDetail(curr->getPath() + "' is group when array or list is expected");
          throw exception;
        }
        else if (ind < 0 or ind >= curr->getLength())
        {
          if (mandatory)
          {
            SmartMet::Spine::Exception exception(BCP, "Index in path is out of range!");
            exception.addParameter("Configuration file", file_name);
            exception.addParameter("Path", format_path(&search_start, path));
            exception.addParameter("Index", std::to_string(ind));
            exception.addParameter("Range", "0 .. " + std::to_string(curr->getLength() - 1));
            throw exception;
          }
          else
          {
            return NULL;
          }
        }
        else
        {
          curr = &(*curr)[ind];
        }
      }
      else
      {
        if (curr->isGroup())
        {
          if (curr->exists(name))
          {
            curr = &(*curr)[name];
          }
          else if (mandatory)
          {
            SmartMet::Spine::Exception exception(BCP, "Path not found!");
            exception.addParameter("Configuration file", file_name);
            exception.addParameter("Path", format_path(curr, name));
            throw exception;
          }
          else
          {
            return NULL;
          }
        }
        else
        {
          SmartMet::Spine::Exception exception(BCP, "Incorrect path!");
          exception.addParameter("Configuration file", file_name);
          exception.addParameter("Path", format_path(curr, name));
          exception.addDetail(curr->getPath() + "' is not a group");
          throw exception;
        }
      }
    }

    return curr;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Spine
}  // namespace SmartMet
