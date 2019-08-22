#include "LogRange.h"
#include "HandlerView.h"

namespace SmartMet
{
namespace Spine
{
LogRange::~LogRange()
{
  itsHandlerView->releaseLogRange();  // decrements reader count
}

LogRange::LogRange(const LogListType& theLog, HandlerView* theHandler)
    : itsBegin(theLog.begin()), itsEnd(theLog.end()), itsHandlerView(theHandler)
{
}

LogRange::LogRange(const LogRange& theOther)
    : itsBegin(theOther.itsBegin), itsEnd(theOther.itsEnd), itsHandlerView(theOther.itsHandlerView)
{
  itsHandlerView->lockLogRange();
}

LogRange::const_iterator LogRange::begin() const
{
  return itsBegin;
}
LogRange::const_iterator LogRange::end() const
{
  return itsEnd;
}

}  // namespace Spine
}  // namespace SmartMet
