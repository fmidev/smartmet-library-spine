#include "IPFilter.h"
#include "ConfigTools.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/Exception.h>
#include <algorithm>
#include <vector>

namespace SmartMet
{
namespace Spine
{
namespace IPFilter
{
namespace
{
using Rule = IPFilter::Rule;

// Convert to the 16 byte form used for matching. IPv4 addresses are IPv4-mapped.
boost::asio::ip::address_v6::bytes_type to_bytes(const boost::asio::ip::address& ip)
{
  if (ip.is_v4())
    return boost::asio::ip::make_address_v6(boost::asio::ip::v4_mapped, ip.to_v4()).to_bytes();
  return ip.to_v6().to_bytes();
}

std::optional<boost::asio::ip::address> make_address(const std::string& str)
{
  boost::system::error_code ec;
  auto ip = boost::asio::ip::make_address(str, ec);
  if (ec)
    return {};
  if (ip.is_v6() && ip.to_v6().is_v4_mapped())
    return boost::asio::ip::address(
        boost::asio::ip::make_address_v4(boost::asio::ip::v4_mapped, ip.to_v6()));
  return ip;
}

// Parse a decimal number with at most the given number of digits
std::optional<unsigned int> parse_number(const std::string& str, std::size_t maxdigits)
{
  if (str.empty() || str.size() > maxdigits || !boost::algorithm::all(str, boost::is_digit()))
    return {};
  return static_cast<unsigned int>(std::stoul(str));
}

// Rule matching exactly the given address prefix
Rule make_prefix_rule(const boost::asio::ip::address& ip, unsigned int prefix)
{
  const auto bytes = to_bytes(ip);
  Rule rule;
  for (std::size_t i = 0; i < rule.size(); i++)
  {
    const unsigned int bits = std::min(8U, prefix - std::min(prefix, 8U * static_cast<unsigned int>(i)));
    const auto mask = static_cast<std::uint8_t>(0xFF00U >> bits);
    rule[i] = {static_cast<std::uint8_t>(bytes[i] & mask),
               static_cast<std::uint8_t>(bytes[i] | static_cast<std::uint8_t>(~mask))};
  }
  return rule;
}

// Legacy IPv4 pattern like 192.168.14-18.*
std::optional<Rule> make_pattern_rule(const std::string& str)
{
  std::vector<std::string> parts;
  boost::algorithm::split(parts, str, boost::is_any_of("."));
  if (parts.size() != 4)
    return {};

  // IPv4-mapped prefix ::ffff:0:0/96
  Rule rule;
  for (std::size_t i = 0; i < 10; i++)
    rule[i] = {0, 0};
  rule[10] = {0xFF, 0xFF};
  rule[11] = {0xFF, 0xFF};

  for (std::size_t i = 0; i < 4; i++)
  {
    const auto& part = parts[i];
    unsigned int lo = 0;
    unsigned int hi = 255;
    if (part != "*")
    {
      const auto pos = part.find('-');
      auto first = parse_number(part.substr(0, pos), 3);
      auto last = (pos == std::string::npos ? first : parse_number(part.substr(pos + 1), 3));
      if (!first || !last || *first > 255 || *last > 255)
        return {};
      lo = std::min(*first, *last);
      hi = std::max(*first, *last);
    }
    rule[12 + i] = {static_cast<std::uint8_t>(lo), static_cast<std::uint8_t>(hi)};
  }
  return rule;
}

Rule make_rule(const std::string& rulestr)
{
  const auto str = boost::algorithm::trim_copy(rulestr);

  const auto slash = str.find('/');
  if (slash != std::string::npos)
  {
    auto ip = make_address(str.substr(0, slash));
    auto prefix = parse_number(str.substr(slash + 1), 3);
    const unsigned int maxprefix = (ip && ip->is_v4() ? 32 : 128);
    if (!ip || !prefix || *prefix > maxprefix)
      throw Fmi::Exception(BCP, "Invalid CIDR IP filter rule: '" + rulestr + "'");
    return make_prefix_rule(*ip, *prefix + (ip->is_v4() ? 96 : 0));
  }

  if (auto ip = make_address(str))
    return make_prefix_rule(*ip, 128);

  if (auto rule = make_pattern_rule(str))
    return *rule;

  throw Fmi::Exception(BCP, "Invalid IP filter rule: '" + rulestr + "'");
}

}  // namespace

std::optional<boost::asio::ip::address> parseAddress(const std::string& ip)
{
  const auto str = boost::algorithm::trim_copy(ip);

  // [v6] or [v6]:port
  if (!str.empty() && str.front() == '[')
  {
    const auto end = str.find(']');
    if (end == std::string::npos)
      return {};
    const auto rest = str.substr(end + 1);
    if (!rest.empty() && (rest[0] != ':' || !parse_number(rest.substr(1), 5)))
      return {};
    auto addr = make_address(str.substr(1, end - 1));
    if (!addr || addr->is_v4())
      return {};
    return addr;
  }

  // a.b.c.d:port
  const auto colon = str.find(':');
  if (colon != std::string::npos && str.find(':', colon + 1) == std::string::npos)
  {
    if (!parse_number(str.substr(colon + 1), 5))
      return {};
    auto addr = make_address(str.substr(0, colon));
    if (!addr || !addr->is_v4())
      return {};
    return addr;
  }

  return make_address(str);
}

IPFilter::IPFilter(const std::vector<std::string>& rules)
try
{
  for (const auto& rule : rules)
    itsRules.push_back(make_rule(rule));
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Failed to construct IP filter");
}

std::shared_ptr<IPFilter> IPFilter::fromConfig(const libconfig::Config& config,
                                               const std::string& path)
try
{
  std::vector<std::string> rules;
  lookupHostStringSettings(config, rules, path);
  if (rules.empty())
    return {};
  return std::make_shared<IPFilter>(rules);
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Failed to read IP filter").addParameter("setting", path);
}

bool IPFilter::match(const boost::asio::ip::address& ip) const
{
  const auto bytes = to_bytes(ip);

  for (const auto& rule : itsRules)
  {
    bool ok = true;
    for (std::size_t i = 0; ok && i < bytes.size(); i++)
      ok = (rule[i].first <= bytes[i] && bytes[i] <= rule[i].second);
    if (ok)
      return true;
  }
  return false;
}

bool IPFilter::match(const std::string& ip) const
{
  // Note: no port numbers or brackets here, only plain addresses are accepted
  auto addr = make_address(ip);
  return addr && match(*addr);
}

std::string resolveClientIP(const std::string& peerIP,
                            const std::optional<std::string>& forwardedFor,
                            const IPFilter& trustedProxies)
try
{
  if (!forwardedFor || !trustedProxies.match(peerIP))
    return peerIP;

  std::vector<std::string> hops;
  boost::algorithm::split(hops, *forwardedFor, boost::is_any_of(","));

  std::string client = peerIP;
  for (auto it = hops.rbegin(); it != hops.rend(); ++it)
  {
    auto addr = parseAddress(*it);
    if (!addr)
      return "unknown";
    client = addr->to_string();
    if (!trustedProxies.match(*addr))
      break;
  }
  return client;
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Failed to resolve client IP");
}

}  // namespace IPFilter
}  // namespace Spine
}  // namespace SmartMet
