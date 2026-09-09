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
                                       std::size_t thePieceLimit,
                                       Duration theMaxGap,
                                       Duration theMaxHold)
    : itsStreamer(std::move(theStreamer)),
      itsMinChunkSize(theMinChunkSize),
      itsPieceLimit(thePieceLimit),
      itsMaxGap(theMaxGap),
      itsMaxHold(theMaxHold)
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

    // When the first piece of this chunk arrived, and so when the bytes now in
    // hand started waiting. Only meaningful once there is a piece.
    TimePoint collectingSince;

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

      const bool timed = (itsMaxGap > Duration::zero() || itsMaxHold > Duration::zero());
      const TimePoint before = timed ? now() : TimePoint{};

      const std::string piece = itsStreamer->getChunk();
      const auto status = itsStreamer->getStatus();

      const TimePoint after = timed ? now() : TimePoint{};

      coalesced += piece;
      if (!piece.empty())
      {
        if (pieces == 0)
          collectingSince = after;
        ++pieces;
      }

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

      // This piece took long enough that the next one probably will too, and
      // holding what is in hand for that long buys a fuller chunk at a price
      // the chunk is not worth. Judged from the call that just returned,
      // because a pull API gives no way to abandon one in progress.
      if (itsMaxGap > Duration::zero() && (after - before) >= itsMaxGap)
        return coalesced;

      // And however fast the pieces come, collected bytes do not wait longer
      // than this. Measured from the first piece of this chunk: what came
      // before it was the producer's own time, with nothing yet in hand.
      if (itsMaxHold > Duration::zero() && (after - collectingSince) >= itsMaxHold)
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
