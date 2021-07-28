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
  std::string adjustField = "right";
  std::string floatField = "fixed";
  int width = -1;
  char fill = ' ';
  bool showPos = false;
  bool upperCase = false;

  // c++14 would not need this, c++11 does because of the defaults above
  ValueFormatterParam(const std::string& theMissingText,
                      const std::string& theAdjustField,
                      const std::string& theFloatField,
                      int theWidth,
                      char theFill,
                      bool theShowPos,
                      bool theUpperCase);

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
  void buildFormat(const ValueFormatterParam& param);
  std::string itsFormat;           // when no precision is given
  std::string itsPrecisionFormat;  // when precision is given
  std::string itsFloatField;       // saved to enable school type rounding fixes
  std::string itsMissingText;      // text for NaN
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
