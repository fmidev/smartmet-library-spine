#pragma once

#include "HTTP.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

namespace SmartMet
{
namespace Spine
{
namespace HTTP
{
// ----------------------------------------------------------------------
/*!
 * \brief Collect a streamer's small pieces into chunks worth sending
 *
 * A ContentStreamer produces whatever unit its producer works in - one GRIB
 * message, one grid, one block read from a file - and the server frames each
 * piece it is handed as one chunk, with its own hex length, its own write and
 * quite possibly its own packet. For a producer whose natural unit is small
 * that is mostly overhead, which is why streamers have grown their own
 * collecting loops (see the download plugin). This is that loop, once, so the
 * streamers can go back to producing their natural unit.
 *
 * It is a plain ContentStreamer wrapping another one, so it can be dropped in
 * wherever a streamer is expected, and it keeps the streamer protocol exactly
 * as the server implements it:
 *
 *   - the wrapped streamer's status is read *before* asking it for anything,
 *     and a status that is no longer OK ends the stream;
 *   - a streamer may return its last bytes in the very call that sets EXIT_OK,
 *     which nearly all of them do, so those bytes are part of the chunk this
 *     returns rather than something to be dropped;
 *   - an empty piece with an OK status means "nothing ready yet, come back
 *     later" - the one thing collecting must never wait out. Whatever has been
 *     collected is flushed immediately instead, so a slow producer streams as
 *     incrementally as it always did and a gateway streamer waiting on a
 *     backend is not turned into a stall.
 *
 * Four limits stop the collecting, and a chunk is sent when the first of them
 * is reached:
 *
 *   - **size**: collecting stops at the first piece that brings the total to
 *     theMinChunkSize, so at most one piece more than the threshold is held;
 *   - **count**: thePieceLimit bounds the number of calls made rather than the
 *     bytes collected, for a producer whose pieces are tiny;
 *   - **gap**: a piece that took theMaxGap or longer to arrive is the last one
 *     collected. A producer that was slow once will likely be slow again, and
 *     waiting for another piece would hold what is already in hand for as long
 *     again - while the framing that collecting saves is noise next to that
 *     much production time. This is judged after the fact, since a pull API
 *     cannot cut a slow call short;
 *   - **hold**: theMaxHold bounds how long collected bytes wait, measured from
 *     the arrival of the first piece of this chunk. Before that there is
 *     nothing being held, so nothing to bound - a producer that takes minutes
 *     to make its first piece is dealt with by the gap limit, which sends that
 *     piece on its own.
 *
 * Any limit set to zero is not applied; with the size or count limit at zero
 * there is no collecting at all and every piece is passed straight on.
 *
 * The verdict is per chunk, not sticky: a producer that starts slowly and then
 * speeds up has its later chunks collected normally.
 */
// ----------------------------------------------------------------------

class CoalescingStreamer : public ContentStreamer
{
 public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;
  using Duration = Clock::duration;

  CoalescingStreamer(std::shared_ptr<ContentStreamer> theStreamer,
                     std::size_t theMinChunkSize,
                     std::size_t thePieceLimit,
                     Duration theMaxGap = std::chrono::milliseconds(500),
                     Duration theMaxHold = std::chrono::milliseconds(1000));

  std::string getChunk() override;

  //! The wrapped streamer, for a caller that has to reach past the wrapper
  const std::shared_ptr<ContentStreamer>& getWrappedStreamer() const { return itsStreamer; }

 protected:
  // ----------------------------------------------------------------------
  /*!
   * \brief The clock the gap and hold limits are measured with
   *
   * Virtual so that a test can decide what time it is instead of sleeping for
   * it: the limits are then exact rather than approximate, and the tests take
   * no longer than any others.
   */
  // ----------------------------------------------------------------------
  virtual TimePoint now() const { return Clock::now(); }

 private:
  std::shared_ptr<ContentStreamer> itsStreamer;
  std::size_t itsMinChunkSize;
  std::size_t itsPieceLimit;
  Duration itsMaxGap;
  Duration itsMaxHold;
};

}  // namespace HTTP
}  // namespace Spine
}  // namespace SmartMet
