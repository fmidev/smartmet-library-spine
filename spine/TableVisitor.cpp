#include "TableVisitor.h"
#include <macgyver/Exception.h>
#include <newbase/NFmiGlobals.h>

namespace SmartMet
{
namespace Spine
{
/* TableVisitor */

void TableVisitor::operator()(const TS::None& /* none */)
{
  try
  {
    itsTable.set(itsCurrentColumn, itsCurrentRow++, itsValueFormatter.missing());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void TableVisitor::operator()(double d)
{
  try
  {
    if (d == static_cast<double>(kFloatMissing))
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void TableVisitor::operator()(const TS::LonLat& lonlat)
{
  try
  {
    std::string res;
    switch (itsLonLatFormat)
    {
      case TS::LonLatFormat::LATLON:
        res.append(itsValueFormatter.format(lonlat.lat, itsPrecisions[itsCurrentColumn]))
            .append(", ")
            .append(itsValueFormatter.format(lonlat.lon, itsPrecisions[itsCurrentColumn]));
        break;

      case TS::LonLatFormat::LONLAT:
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TableVisitor& operator<<(TableVisitor& tf, const TS::Value& val)
{
  try
  {
    if (boost::get<int>(&val) != nullptr)
      tf << *(boost::get<int>(&val));
    else if (boost::get<double>(&val) != nullptr)
      tf << *(boost::get<double>(&val));
    else if (boost::get<std::string>(&val) != nullptr)
      tf << *(boost::get<std::string>(&val));
    else if (boost::get<TS::LonLat>(&val) != nullptr)
    {
      TS::LonLat coord = *(boost::get<TS::LonLat>(&val));
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

TableVisitor& TableVisitor::operator<<(TS::LonLatFormat newformat)
{
  try
  {
    itsLonLatFormat = newformat;
    return *this;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet
