#include "TableVisitor.h"
#include <macgyver/Exception.h>
#include <newbase/NFmiGlobals.h>

namespace SmartMet
{
namespace Spine
{
/* TableVisitor */

void TableVisitor::operator()(const None& /* none */)
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void TableVisitor::operator()(const Fmi::LocalDateTime& ldt)
{
  try
  {
    std::string res;

    if (ldt.is_not_a_date_time())
      res = itsValueFormatter.missing();
    else if (itsTimeFormatter != nullptr)
    {
      if (itsTimeZonePtr)
        res = itsTimeFormatter->format(ldt.local_time_in(*itsTimeZonePtr));
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

TableVisitor& TableVisitor::operator<<(LonLatFormat newformat)
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
