#include "CoalescingStreamer.h"

#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Spine
{
namespace HTTP
{
CoalescingStreamer::CoalescingStreamer(std::shared_ptr<ContentStreamer> theStreamer,
                                       std::size_t theMinChunkSize,
                                       std::size_t thePieceLimit)
    : itsStreamer(std::move(theStreamer)),
      itsMinChunkSize(theMinChunkSize),
      itsPieceLimit(thePieceLimit)
{
  if (!itsStreamer)
    throw Fmi::Exception(BCP, "Cannot coalesce the chunks of a streamer that does not exist");
}

std::string CoalescingStreamer::getChunk()
{
  try
  {
    std::string coalesced;
    std::size_t pieces = 0;

    while (true)
    {
      // The status is what says whether there is anything left to ask for, and
      // it is read before asking - the same order the server uses, and the
      // reason a streamer may return data in the call that finishes it.
      if (itsStreamer->getStatus() != StreamerStatus::OK)
      {
        setStatus(itsStreamer->getStatus());
        return coalesced;
      }

      const std::string piece = itsStreamer->getChunk();
      const auto status = itsStreamer->getStatus();

      coalesced += piece;
      if (!piece.empty())
        ++pieces;

      if (status != StreamerStatus::OK)
      {
        // The wrapped streamer ended in this very call, and its last bytes -
        // where there are any - came back with it. They are returned here
        // together with the status, which is what the server expects: it sends
        // the content it was handed and acts on the status when it next asks.
        //
        // An EXIT_ERROR carries whatever was collected too. The message is
        // broken either way, and the server leaves such a body unterminated, so
        // there is nothing to be gained by also discarding bytes the producer
        // had already handed over.
        setStatus(status);
        return coalesced;
      }

      if (piece.empty())
      {
        // Nothing ready yet. Waiting for more would be waiting on the producer,
        // so this flushes instead: with content, the server sends it; without,
        // the server reschedules and asks again.
        return coalesced;
      }

      if (coalesced.size() >= itsMinChunkSize || pieces >= itsPieceLimit)
        return coalesced;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace HTTP
}  // namespace Spine
}  // namespace SmartMet
