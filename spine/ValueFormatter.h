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
  std::string missingText;
  std::string adjustField;
  std::string floatField;
  int width;
  char fill;
  bool showPos;
  bool upperCase;

  ValueFormatterParam();
};

class ValueFormatter
{
 public:
  ValueFormatter(const HTTP::Request& theReq);
  ValueFormatter(const ValueFormatterParam& param);

  std::string format(double theValue, int thePrecision) const;
  const std::string& missing() const { return itsMissingText; }
  void setMissingText(const std::string& theMissingText) { itsMissingText = theMissingText; }

 private:
  ValueFormatter();
  boost::shared_ptr<std::ostringstream> itsStream;  // not safe to be used from multiple threads
  std::string itsMissingText;
  int itsWidth;
  char itsFill;
  std::string itsAdjustField;
  bool itsShowPos;
  bool itsUpperCase;
  std::string itsFloatField;
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
