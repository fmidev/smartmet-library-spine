// ======================================================================
/*!
 * \brief Options for generating a time series
 */
// ======================================================================

#include "TimeSeriesGeneratorOptions.h"
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <macgyver/StringConversion.h>

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief The default constructor marks most options as missing
 *
 * The moment of construction determines what "now" means as far
 * as time generation is concerned, unless the value is overwritten
 * after construction.
 */
// ----------------------------------------------------------------------

TimeSeriesGeneratorOptions::TimeSeriesGeneratorOptions(const boost::posix_time::ptime& now)
    : mode(Mode::TimeSteps),
      startTime(now),
      endTime(now),
      startTimeUTC(true),
      endTimeUTC(true),
      dataTimes(new TimeList::element_type()),
      startTimeData(false),
      endTimeData(false),
      isClimatology(false)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate a unique hash for the options
 */
// ----------------------------------------------------------------------

std::size_t TimeSeriesGeneratorOptions::hash_value() const
{
  try
  {
    std::size_t hash = 0;
    Fmi::hash_combine(hash, Fmi::hash_value(static_cast<int>(mode)));
    Fmi::hash_combine(hash, Fmi::hash_value(Fmi::to_iso_string(startTime)));
    Fmi::hash_combine(hash, Fmi::hash_value(Fmi::to_iso_string(endTime)));
    Fmi::hash_combine(hash, Fmi::hash_value(startTimeUTC));
    Fmi::hash_combine(hash, Fmi::hash_value(endTimeUTC));
    if (timeSteps)
    {
      Fmi::hash_combine(hash, Fmi::hash_value(*timeSteps));
    }
    if (timeStep)
    {
      Fmi::hash_combine(hash, Fmi::hash_value(*timeStep));
    }
    for (const auto& t : timeList)
      Fmi::hash_combine(hash, Fmi::hash_value(t));
    Fmi::hash_combine(hash, Fmi::hash_value(*dataTimes));
    Fmi::hash_combine(hash, Fmi::hash_value(startTimeData));
    Fmi::hash_combine(hash, Fmi::hash_value(endTimeData));
    Fmi::hash_combine(hash, Fmi::hash_value(isClimatology));
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if all available timesteps are to be output
 */
// ----------------------------------------------------------------------

bool TimeSeriesGeneratorOptions::all() const
{
  try
  {
    switch (mode)
    {
      case DataTimes:
      case GraphTimes:
        return true;
      case FixedTimes:
        return false;
      case TimeSteps:
        return (!timeStep || *timeStep == 0);
    }
    // Compilers cannot see the above handles all cases
    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Print the options to the given stream
 *
 * This is needed for debugging purposes only.
 */
// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& stream, const TimeSeriesGeneratorOptions& opt)
{
  try
  {
    stream << "TimeSeriesGeneratorOptions:\n";
    stream << "    mode          : ";  // << static_cast<int>(opt.mode) << "\n";
    switch (opt.mode)
    {
      case TimeSeriesGeneratorOptions::DataTimes:
        stream << "DataTimes";
        break;
      case TimeSeriesGeneratorOptions::GraphTimes:
        stream << "GraphTiems";
        break;
      case TimeSeriesGeneratorOptions::FixedTimes:
        stream << "FixedTimes";
        break;
      case TimeSeriesGeneratorOptions::TimeSteps:
        stream << "TimeSteps";
        break;
    }
    stream << "\n    startTime     : " << opt.startTime << "\n";
    stream << "    endTime       : " << opt.endTime << "\n";
    stream << "    startTimeUTC  : " << opt.startTimeUTC << "\n";
    stream << "    endTimeUTC    : " << opt.endTimeUTC << "\n";
    stream << "    timeSteps     : ";
    if (opt.timeSteps)
      stream << *opt.timeSteps;
    stream << "\n    timeStep      : ";
    if (opt.timeStep)
      stream << *opt.timeStep;
    stream << "\n    dataTimes     : ";
    stream << "\n    startTimeData : " << opt.startTimeData << "\n";
    stream << "\n    endTimeData   : " << opt.endTimeData << "\n";
    std::copy(opt.timeList.begin(), opt.timeList.end(), std::ostream_iterator<int>(stream, " "));
    stream << "\n    dataTimes     : ";
    for (const auto& t : *opt.getDataTimes())
    {
      stream << " '" << t << "'";
    }
    stream << "\n";

    return stream;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the data times
 */
// ----------------------------------------------------------------------

void TimeSeriesGeneratorOptions::setDataTimes(const TimeList& times, bool climatology)
{
  try
  {
    dataTimes = times;
    isClimatology = climatology;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the data times
 */
// ----------------------------------------------------------------------

const TimeSeriesGeneratorOptions::TimeList& TimeSeriesGeneratorOptions::getDataTimes() const
{
  return dataTimes;
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
