
#include "IPFilter.h"
#include <macgyver/Exception.h>
#include <boost/algorithm/string.hpp>
#include <boost/spirit/include/qi.hpp>

#include <vector>

namespace SmartMet
{
namespace Spine
{
namespace IPFilter
{
SequenceFilter::~SequenceFilter() {}

SequenceFilterPtr makeFilter(const std::string& formatToken)
{
  try
  {
    if (formatToken == "*")
    {
      return SequenceFilterPtr(new AnyFilter(formatToken));
    }
    else if (formatToken.find("-") != std::string::npos)
    {
      return SequenceFilterPtr(new RangeFilter(formatToken));
    }
    else if (boost::algorithm::all(formatToken, boost::is_digit()))
    {
      return SequenceFilterPtr(new SingleFilter(formatToken));
    }
    else
    {
      throw Fmi::Exception(BCP, "Unrecognized format token: " + formatToken);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

AnyFilter::AnyFilter(const std::string& /* format */) {}

bool AnyFilter::match(const std::string& /* sequence */) const
{
  // Matches all sequences
  return true;
}

SingleFilter::SingleFilter(const std::string& format) : itsMatch(format) {}

bool SingleFilter::match(const std::string& sequence) const
{
  // Matches if match is exact
  if (sequence == itsMatch)
  {
    return true;
  }
  else
  {
    return false;
  }
}

RangeFilter::RangeFilter(const std::string& format)
{
  try
  {
    std::vector<std::string> limits;
    limits.reserve(2);
    boost::algorithm::split(limits, format, boost::is_any_of("-"));

    if (limits.size() != 2)
    {
      throw Fmi::Exception(BCP, "Invalid range filter construction format: " + format);
    }

    unsigned long first = std::strtoul(limits[0].c_str(), nullptr, 10);

    unsigned long second = std::strtoul(limits[1].c_str(), nullptr, 10);

    if (first > second)
    {
      itsHighLimit = first;
      itsLowLimit = second;
    }
    else
    {
      itsHighLimit = second;
      itsLowLimit = first;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool RangeFilter::match(const std::string& sequence) const
{
  try
  {
    unsigned long compare = std::strtoul(sequence.c_str(), nullptr, 10);

    if ((itsLowLimit <= compare) && (compare <= itsHighLimit))
    {
      return true;
    }
    else
    {
      return false;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

AddressFilter::AddressFilter(const std::string& formatString)
{
  try
  {
    std::vector<std::string> tokens;
    tokens.reserve(4);
    boost::algorithm::split(tokens, formatString, boost::is_any_of("."));

    if (tokens.size() != 4)
    {
      throw Fmi::Exception(BCP, "Invalid IP filter format string: " + formatString);
    }

    unsigned int index = 0;
    for (auto& token : tokens)
    {
      itsFilters[index] = makeFilter(token);
      ++index;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool AddressFilter::match(const std::vector<std::string>& ipTokens) const
{
  try
  {
    // If for some reason ip has more than 4 fields, the following will segfault.
    // IPs however come from an boost-asio, so we assume they are OK
    unsigned int index = 0;
    for (auto& token : ipTokens)
    {
      bool success = itsFilters[index]->match(token);
      if (!success)
      {
        // One miss is all we need
        return false;
      }

      ++index;
    }

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

IPConfig::~IPConfig() {}

IPConfig::IPConfig(const std::string& configFile, const std::string& root) : ConfigBase(configFile)
{
  try
  {
    std::vector<std::string> matchTokens;

    bool success = false;
    if (root == "")
    {
      success = get_config_array(get_root(), "ip_filters", matchTokens);
    }
    else
    {
      success = get_config_array(root + ".ip_filters", matchTokens);
    }

    if (!success)
    {
      throw Fmi::Exception(BCP,
                             "Group '" + (root.empty() ? "ip_filters" : (root + ".ip_filters")) +
                                 "' not found in configuration");
    }

    itsMatchTokens = matchTokens;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

IPConfig::IPConfig(boost::shared_ptr<libconfig::Config> configPtr, const std::string& root)
    : ConfigBase(configPtr)
{
  try
  {
    std::vector<std::string> matchTokens;

    bool success = false;
    if (root.empty())
    {
      success = get_config_array(get_root(), "ip_filters", matchTokens);
    }
    else
    {
      success = get_config_array(root + ".ip_filters", matchTokens);
    }

    if (!success)
    {
      throw Fmi::Exception(BCP,
                             "Group '" + (root.empty() ? "ip_filters" : (root + ".ip_filters")) +
                                 "' not found in configuration");
    }

    itsMatchTokens = matchTokens;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

const std::vector<std::string>& IPConfig::getTokens() const
{
  return itsMatchTokens;
}

IPFilter::IPFilter(const std::string& configFile, const std::string& root)
    : itsConfig(new IPConfig(configFile, root))
{
  try
  {
    const auto theTokens = itsConfig->getTokens();
    for (auto& formatToken : theTokens)
    {
      itsFilters.emplace_back(formatToken);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

IPFilter::IPFilter(boost::shared_ptr<libconfig::Config> configPtr, const std::string& root)
    : itsConfig(new IPConfig(configPtr, root))
{
  try
  {
    const auto theTokens = itsConfig->getTokens();
    for (auto& formatToken : theTokens)
    {
      itsFilters.emplace_back(formatToken);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

IPFilter::IPFilter(const std::vector<std::string>& formatTokens) : itsConfig()
{
  try
  {
    for (auto& formatToken : formatTokens)
    {
      itsFilters.emplace_back(formatToken);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool IPFilter::match(const std::string& ip) const
{
  try
  {
    std::vector<std::string> ipTokens;
    ipTokens.reserve(4);
    boost::algorithm::split(ipTokens, ip, boost::is_any_of("."));

    // Here is an implicit assumption that ipTokens.size() == 4

    // See if given ip matches ANY of the given rules
    for (auto& filter : itsFilters)
    {
      if (filter.match(ipTokens))
      {
        // Return true if any of the filters is matched
        return true;
      }
    }
    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace IPFilter
}  // namespace Spine
}  // namespace SmartMet
