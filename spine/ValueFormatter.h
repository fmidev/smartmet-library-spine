// ======================================================================
/*!
 * \brief Interface of class Valueformatter
 */
// ======================================================================

#pragma once

#include "HTTP.h"
#include <boost/shared_ptr.hpp>
#include <sstream>
#include <string>

namespace SmartMet
{
namespace Spine
{
struct ValueFormatterParam
{
  std::string missingText = "nan";
  std::string floatField = "fixed";

  // c++14 would not need this, c++11 does because of the defaults above
  ValueFormatterParam(const std::string& theMissingText, const std::string& theFloatField);

  // and this is needed because above is needed
  ValueFormatterParam() = default;
  ValueFormatterParam(const ValueFormatterParam& theOther) = default;
};

class ValueFormatter
{
 public:
  ValueFormatter(const HTTP::Request& theReq);
  ValueFormatter(const ValueFormatterParam& param);
  ValueFormatter() = delete;

  std::string format(double theValue, int thePrecision) const;
  const std::string& missing() const { return itsMissingText; }
  void setMissingText(const std::string& theMissingText) { itsMissingText = theMissingText; }

 private:
  std::string format_double_conversion_fixed(double theValue, int thePrecision) const;
  std::string format_double_conversion_scientific(double theValue, int thePrecision) const;
  std::string itsFloatField;   // saved to enable school type rounding fixes
  std::string itsMissingText;  // text for NaN
  ValueFormatterParam itsFormatterParam;
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
