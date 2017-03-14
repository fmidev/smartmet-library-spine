#include "TimeSeriesOutput.h"
#include "Exception.h"

#include <newbase/NFmiGlobals.h>

#include <macgyver/StringConversion.h>

#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/format.hpp>

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
    if (boost::get<double>(&val))
      os << *(boost::get<double>(&val));
    else if (boost::get<int>(&val))
      os << *(boost::get<int>(&val));
    else if (boost::get<std::string>(&val))
      os << *(boost::get<std::string>(&val));
    else if (boost::get<std::pair<double, double> >(&val))
    {
      LonLat coord = *(boost::get<LonLat>(&val));
      os << coord;
    }
    else if (boost::get<boost::local_time::local_date_time>(&val))
    {
      boost::local_time::local_date_time ldt =
          *(boost::get<boost::local_time::local_date_time>(&val));
      os << ldt;
    }
    else if (boost::get<None>(&val))
    {
      os << "nan";
    }

    return os;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::ostream& operator<<(std::ostream& os, const TimeSeries& ts)
{
  try
  {
    for (unsigned int i = 0; i < ts.size(); i++)
      os << ts[i].time << " -> " << ts[i].value << std::endl;

    return os;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// StringVisitor

std::string StringVisitor::operator()(const None& none) const
{
  try
  {
    return itsValueFormatter.missing();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

/* OStreamVisitor */
void OStreamVisitor::operator()(const None& none) const
{
  try
  {
    itsOutstream << itsValueFormatter.missing();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

/* TableVisitor */

void TableVisitor::operator()(const None& none)
{
  try
  {
    itsTable.set(itsCurrentColumn, itsCurrentRow++, itsValueFormatter.missing());
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void TableVisitor::operator()(const boost::local_time::local_date_time& ldt)
{
  try
  {
    std::string res;

    if (ldt.is_not_a_date_time())
      res = itsValueFormatter.missing();
    else if (itsTimeFormatter.get())
    {
      if (itsTimeZonePtr)
        res = itsTimeFormatter.get()->format(ldt.local_time_in(itsTimeZonePtr.get()));
      else
        res = itsTimeFormatter.get()->format(ldt.utc_time());
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

TableVisitor& operator<<(TableVisitor& tf, const Value& val)
{
  try
  {
    if (boost::get<int>(&val))
      tf << *(boost::get<int>(&val));
    else if (boost::get<double>(&val))
      tf << *(boost::get<double>(&val));
    else if (boost::get<std::string>(&val))
      tf << *(boost::get<std::string>(&val));
    else if (boost::get<std::pair<double, double> >(&val))
    {
      LonLat coord = *(boost::get<LonLat>(&val));
      tf << coord;
    }
    else if (boost::get<boost::local_time::local_date_time>(&val))
    {
      boost::local_time::local_date_time ldt =
          *(boost::get<boost::local_time::local_date_time>(&val));
      tf << ldt;
    }

    return tf;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

TableVisitor& TableVisitor::operator<<(LonLatFormat newformat)
{
  try
  {
    this->itsLonLatFormat = newformat;
    return *this;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace TimeSeries
}  // namespace Spine
}  // namespace SmartMet
