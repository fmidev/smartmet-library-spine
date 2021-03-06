#pragma once

#include "Location.h"
#include "OptionParsers.h"
#include "Parameter.h"
#include "TimeSeries.h"
#include "ValueFormatter.h"
#include <macgyver/Exception.h>
#include <macgyver/TimeFormatter.h>
#include <macgyver/TimeZones.h>

namespace SmartMet
{
namespace Spine
{
bool special(const Parameter& theParam);

bool parameter_is_arithmetic(const Spine::Parameter& theParameter);

bool is_plain_location_query(const Spine::OptionParsers::ParameterList& theParams);

bool is_location_parameter(const std::string& paramname);

bool is_time_parameter(std::string paramname);

std::string location_parameter(const Spine::LocationPtr loc,
                               const std::string paramName,
                               const Spine::ValueFormatter& valueformatter,
                               const std::string& timezone,
                               int precision);

Spine::TimeSeries::Value time_parameter(const std::string paramname,
                                        const boost::local_time::local_date_time& ldt,
                                        const boost::posix_time::ptime now,
                                        const Spine::Location& loc,
                                        const std::string& timezone,
                                        const Fmi::TimeZones& timezones,
                                        const std::locale& outlocale,
                                        const Fmi::TimeFormatter& timeformatter,
                                        const std::string& timestring);

Parameter makeParameter(const std::string& name);

bool is_special_parameter(const std::string& name);

}  // namespace Spine
}  // namespace SmartMet
