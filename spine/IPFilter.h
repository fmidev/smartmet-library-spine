// ======================================================================
/*!
 * \brief Interface of class IPFilter
 *
 * This class is used to see if an IP address matches a set of rules.
 * It is used for the admin and plugin access filters as well as for
 * deciding which reverse proxies are trusted to report the client IP
 * in an X-Forwarded-For header.
 *
 */
// ======================================================================

#pragma once

#include <boost/asio/ip/address.hpp>
#include <array>
#include <cstdint>
#include <libconfig.h++>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Spine
{
namespace IPFilter
{
// ----------------------------------------------------------------------
/*!
 * \brief User-facing IP filter class
 *
 * Holds zero or more rules, an address is accepted if it matches any
 * of them. An empty filter matches nothing. Accepted rule formats:
 *
 * - IPv4 patterns such as "192.168.14-18.*", where a number matches
 *   itself, a dash (-) matches a range and an asterisk (*) matches any
 *   number
 * - exact addresses such as "10.1.2.3" or "::1"
 * - CIDR blocks such as "10.0.0.0/8" or "fd00::/8"
 *
 * Malformed rules throw during construction. Malformed addresses never
 * match. IPv4-mapped IPv6 addresses (::ffff:a.b.c.d) are matched as
 * IPv4 addresses.
 */
// ----------------------------------------------------------------------
class IPFilter
{
 public:
  explicit IPFilter(const std::vector<std::string>& rules);

  // Build a filter from a string array setting, honouring host specific
  // 'overrides' like other host settings. Returns nullptr if the setting
  // does not exist or is empty.
  static std::shared_ptr<IPFilter> fromConfig(const libconfig::Config& config,
                                              const std::string& path);

  bool match(const std::string& ip) const;
  bool match(const boost::asio::ip::address& ip) const;

  bool empty() const { return itsRules.empty(); }

  // Allowed range for each byte of an IPv6 (or IPv4-mapped) address
  using Rule = std::array<std::pair<std::uint8_t, std::uint8_t>, 16>;

 private:
  std::vector<Rule> itsRules;
};

// ----------------------------------------------------------------------
/*!
 * \brief Parse an address as found in an X-Forwarded-For header
 *
 * Accepts plain IPv4/IPv6 addresses, "a.b.c.d:port", "[v6]" and
 * "[v6]:port". IPv4-mapped IPv6 addresses are converted to IPv4.
 */
// ----------------------------------------------------------------------

std::optional<boost::asio::ip::address> parseAddress(const std::string& ip);

// ----------------------------------------------------------------------
/*!
 * \brief Resolve the client IP of a request
 *
 * The X-Forwarded-For header is believed only when the socket peer is a
 * trusted proxy. The header is then walked from right to left, skipping
 * trusted proxies, and the first untrusted address is the client. If all
 * the addresses are trusted, the left-most one is the client. A malformed
 * entry in the part of the chain being walked yields "unknown", never the
 * address of the proxy which forwarded it.
 */
// ----------------------------------------------------------------------

std::string resolveClientIP(const std::string& peerIP,
                            const std::optional<std::string>& forwardedFor,
                            const IPFilter& trustedProxies);

}  // namespace IPFilter
}  // namespace Spine
}  // namespace SmartMet
