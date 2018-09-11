#include "TimeSeriesOutput.h"
#include "Exception.h"

#include <newbase/NFmiGlobals.h>

#include <macgyver/StringConversion.h>

#include <boost/format.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>

namespace SmartMet
{
namespace Spine
{
namespace TimeSeries
{
std::ostream& operator<<(std::ostream& os, const Value& val)
{
  try
  {
    if (boost::get<double>(&val) != nullptr)
      os << *(boost::get<double>(&val));
    else if (boost::get<int>(&val) != nullptr)
      os << *(boost::get<int>(&val));
    else if (boost::get<std::string>(&val) != nullptr)
      os << *(boost::get<std::string>(&val));
    else if (boost::get<LonLat>(&val) != nullptr)
    {
      LonLat coord = *(boost::get<LonLat>(&val));
      os << coord;
    }
    else if (boost::get<boost::local_time::local_date_time>(&val) != nullptr)
    {
      boost::local_time::local_date_time ldt =
          *(boost::get<boost::local_time::local_date_time>(&val));
      os << ldt;
    }
    else if (boost::get<None>(&val) != nullptr)
    {
      os << "nan";
    }

    return os;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::ostream& operator<<(std::ostream& os, const TimeSeries& ts)
{
  try
  {
    for (const auto& t : ts)
      os << t.time << " -> " << t.value << std::endl;

    return os;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::ostream& operator<<(std::ostream& os, const TimeSeriesGroup& tsg)
{
  try
  {
    for (unsigned int i = 0; i < tsg.size(); i++)
    {
      if (i > 0)
        os << std::endl;
      //		  os << "location #" << i << " (lon, lat): " << tsg[i].lonlat.lon << ", " <<
      // tsg[i].lonlat.lat  << std::endl;
      os << tsg[i].timeseries;
    }

    return os;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::ostream& operator<<(std::ostream& os, const TimeSeriesVector& tsv)
{
  try
  {
    for (unsigned int i = 0; i < tsv.size(); i++)
    {
      os << "column #" << i << std::endl;
      os << tsv[i];
    }

    return os;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// StringVisitor

std::string StringVisitor::operator()(const None& /* none */) const
{
  try
  {
    return itsValueFormatter.missing();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string StringVisitor::operator()(const std::string& str) const
{
  return str;
}

std::string StringVisitor::operator()(double d) const
{
  try
  {
    return itsValueFormatter.format(d, itsPrecision);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string StringVisitor::operator()(int i) const
{
  try
  {
    return Fmi::to_string(i);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string StringVisitor::operator()(const LonLat& lonlat) const
{
  try
  {
    switch (itsLonLatFormat)
    {
      case LonLatFormat::LONLAT:
        return itsValueFormatter.format(lonlat.lon, itsPrecision) + ", " +
               itsValueFormatter.format(lonlat.lat, itsPrecision);
      case LonLatFormat::LATLON:
        return itsValueFormatter.format(lonlat.lat, itsPrecision) + ", " +
               itsValueFormatter.format(lonlat.lon, itsPrecision);
      default:  // Never reached
        return "";
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string StringVisitor::operator()(const boost::local_time::local_date_time& ldt) const
{
  try
  {
    return Fmi::to_iso_extended_string(ldt.local_time());
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

/* OStreamVisitor */
void OStreamVisitor::operator()(const None& /* none */) const
{
  try
  {
    itsOutstream << itsValueFormatter.missing();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void OStreamVisitor::operator()(const std::string& str) const
{
  try
  {
    itsOutstream << str;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void OStreamVisitor::operator()(double d) const
{
  try
  {
    itsOutstream << itsValueFormatter.format(d, itsPrecision);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void OStreamVisitor::operator()(int i) const
{
  try
  {
    itsOutstream << i;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void OStreamVisitor::operator()(const LonLat& lonlat) const
{
  try
  {
    itsOutstream << itsValueFormatter.format(lonlat.lon, itsPrecision) << ", "
                 << itsValueFormatter.format(lonlat.lat, itsPrecision);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void OStreamVisitor::operator()(const boost::local_time::local_date_time& ldt) const
{
  try
  {
    itsOutstream << ldt;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

OStreamVisitor& OStreamVisitor::operator<<(LonLatFormat newformat)
{
  try
  {
    itsLonLatFormat = newformat;
    return *this;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

/* TableVisitor */

void TableVisitor::operator()(const None& /* none */)
{
  try
  {
    itsTable.set(itsCurrentColumn, itsCurrentRow++, itsValueFormatter.missing());
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void TableVisitor::operator()(const std::string& str)
{
  try
  {
    itsTable.set(itsCurrentColumn, itsCurrentRow++, str);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void TableVisitor::operator()(double d)
{
  try
  {
    if (d == kFloatMissing)
    {
      itsTable.set(itsCurrentColumn, itsCurrentRow++, itsValueFormatter.missing());
      return;
    }

    itsTable.set(itsCurrentColumn,
                 itsCurrentRow++,
                 itsValueFormatter.format(d, itsPrecisions[itsCurrentColumn]));
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void TableVisitor::operator()(int i)
{
  try
  {
    itsTable.set(itsCurrentColumn, itsCurrentRow++, std::to_string(i));
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void TableVisitor::operator()(const LonLat& lonlat)
{
  try
  {
    std::string res;
    switch (itsLonLatFormat)
    {
      case LonLatFormat::LATLON:
        res.append(itsValueFormatter.format(lonlat.lat, itsPrecisions[itsCurrentColumn]))
            .append(", ")
            .append(itsValueFormatter.format(lonlat.lon, itsPrecisions[itsCurrentColumn]));
        break;

      case LonLatFormat::LONLAT:
      default:
        res.append(itsValueFormatter.format(lonlat.lon, itsPrecisions[itsCurrentColumn]))
            .append(", ")
            .append(itsValueFormatter.format(lonlat.lat, itsPrecisions[itsCurrentColumn]));
        break;
    }

    itsTable.set(itsCurrentColumn, itsCurrentRow++, res);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void TableVisitor::operator()(const boost::local_time::local_date_time& ldt)
{
  try
  {
    std::string res;

    if (ldt.is_not_a_date_time())
      res = itsValueFormatter.missing();
    else if (itsTimeFormatter != nullptr)
    {
      if (itsTimeZonePtr)
        res = itsTimeFormatter->format(ldt.local_time_in(itsTimeZonePtr.get()));
      else
        res = itsTimeFormatter->format(ldt.utc_time());
    }
    else
    {
      // Fall back to stream formatting
      std::ostringstream ss;
      ss << ldt;
      res = ss.str();
    }

    itsTable.set(itsCurrentColumn, itsCurrentRow++, res);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

TableVisitor& operator<<(TableVisitor& tf, const Value& val)
{
  try
  {
    if (boost::get<int>(&val) != nullptr)
      tf << *(boost::get<int>(&val));
    else if (boost::get<double>(&val) != nullptr)
      tf << *(boost::get<double>(&val));
    else if (boost::get<std::string>(&val) != nullptr)
      tf << *(boost::get<std::string>(&val));
    else if (boost::get<LonLat>(&val) != nullptr)
    {
      LonLat coord = *(boost::get<LonLat>(&val));
      tf << coord;
    }
    else if (boost::get<boost::local_time::local_date_time>(&val) != nullptr)
    {
      boost::local_time::local_date_time ldt =
          *(boost::get<boost::local_time::local_date_time>(&val));
      tf << ldt;
    }

    return tf;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

TableVisitor& TableVisitor::operator<<(LonLatFormat newformat)
{
  try
  {
    itsLonLatFormat = newformat;
    return *this;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace TimeSeries
}  // namespace Spine
}  // namespace SmartMet
