#pragma once

#include "HTTP.h"

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
 * Collecting stops at the first piece that brings the total to theMinChunkSize,
 * so at most one piece more than the threshold is ever held; thePieceLimit
 * bounds the number of calls made instead of the bytes collected, for a
 * producer whose pieces are tiny. Either limit set to zero means no collecting
 * at all: every piece is passed straight on.
 */
// ----------------------------------------------------------------------

class CoalescingStreamer : public ContentStreamer
{
 public:
  CoalescingStreamer(std::shared_ptr<ContentStreamer> theStreamer,
                     std::size_t theMinChunkSize,
                     std::size_t thePieceLimit);

  std::string getChunk() override;

  //! The wrapped streamer, for a caller that has to reach past the wrapper
  const std::shared_ptr<ContentStreamer>& getWrappedStreamer() const { return itsStreamer; }

 private:
  std::shared_ptr<ContentStreamer> itsStreamer;
  std::size_t itsMinChunkSize;
  std::size_t itsPieceLimit;
};

}  // namespace HTTP
}  // namespace Spine
}  // namespace SmartMet
