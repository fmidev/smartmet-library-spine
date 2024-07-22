// ======================================================================
/*!
 * \brief Interface of class IPFilter
 *
 * This class is used to see if the incoming request IP matches a set
 * of allowed ip masks.
 *
 */
// ======================================================================

#pragma once

#include "ConfigBase.h"

#include <string>

#include <array>
#include <boost/scoped_ptr.hpp>
#include <memory>

namespace SmartMet
{
namespace Spine
{
namespace IPFilter
{
// Base class for for filter tokens
class SequenceFilter
{
 public:
  virtual ~SequenceFilter();

  virtual bool match(const std::string& sequence) const = 0;
};

using SequenceFilterPtr = std::shared_ptr<SequenceFilter>;

// Matches any ip token (*)
class AnyFilter : public SequenceFilter
{
 public:
  explicit AnyFilter(const std::string& format);

  bool match(const std::string& sequence) const override;
};

// Matches a single ip token (128)
class SingleFilter : public SequenceFilter
{
 public:
  explicit SingleFilter(std::string format);

  bool match(const std::string& sequence) const override;

 private:
  std::string itsMatch;
};

// Matches a range of  ip tokens (128-255)
class RangeFilter : public SequenceFilter
{
 public:
  explicit RangeFilter(const std::string& format);

  bool match(const std::string& sequence) const override;

 private:
  unsigned long itsLowLimit;

  unsigned long itsHighLimit;
};

// Class that holds 4 sequence filters to make up a complete IP filter
class AddressFilter
{
 public:
  explicit AddressFilter(const std::string& formatString);

  bool match(const std::vector<std::string>& ipTokens) const;

 private:
  std::array<SequenceFilterPtr, 4> itsFilters;
};

// Class for filter configuration
class IPConfig : public ConfigBase
{
 public:
  ~IPConfig() override;

  IPConfig();

  explicit IPConfig(const std::string& configFile, const std::string& root = "");

  explicit IPConfig(const std::shared_ptr<libconfig::Config>& configPtr,
                    const std::string& root = "");

  const std::vector<std::string>& getTokens() const;

 private:
  std::vector<std::string> itsMatchTokens;
};

// ----------------------------------------------------------------------
/*!
 * \brief User-facing IP filter class
 *
 * Holds zero or more IP filtering objects.
 * Constructs using given configuration file and root (in libconfig notation).
 *
 * Filter looks for 'ip_filter' array in the given configuration location
 * (file + root). Array contains tokens in the form of "192.168.14-18.*".
 * where:
 * - Single number matches as single numer
 * - Dash (-) matches a range of numbers
 * - Asterisk (*) matches any number
 */
// ----------------------------------------------------------------------
class IPFilter
{
 public:
  explicit IPFilter(const std::string& configFile, const std::string& root = "");

  explicit IPFilter(const std::shared_ptr<libconfig::Config>& configPtr,
                    const std::string& root = "");

  explicit IPFilter(const std::vector<std::string>& formatTokens);

  bool match(const std::string& ip) const;

 private:
  boost::scoped_ptr<IPConfig> itsConfig;

  std::vector<AddressFilter> itsFilters;
};

}  // namespace IPFilter
}  // namespace Spine
}  // namespace SmartMet
