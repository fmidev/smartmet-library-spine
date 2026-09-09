#include "CoalescingStreamer.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <memory>
#include <string>
#include <vector>

#include <regression/tframe.h>

using SmartMet::Spine::HTTP::CoalescingStreamer;
using SmartMet::Spine::HTTP::ContentStreamer;

using namespace std::chrono_literals;

namespace CoalescingStreamerTest
{
// ----------------------------------------------------------------------
/*!
 * \brief A streamer that hands out a scripted sequence of pieces
 *
 * Every case worth testing here is a sequence: pieces of some size, an empty
 * one where the producer has nothing ready, and an end that either carries the
 * last bytes with it or does not. So the fixture is the sequence itself, and
 * what is asserted is what the wrapper made of it - including how many times it
 * asked, which is the whole point of collecting.
 */
// ----------------------------------------------------------------------

class ScriptedStreamer : public ContentStreamer
{
 public:
  // A piece to return, the status to be in afterwards, and how long producing it
  // took - which the fixture's clock is moved by rather than slept through
  struct Step
  {
    std::string data;
    StreamerStatus status = StreamerStatus::OK;
    std::chrono::milliseconds took{0};
  };

  explicit ScriptedStreamer(std::vector<Step> theSteps) : itsSteps(std::move(theSteps)) {}

  std::string getChunk() override
  {
    ++itsCalls;

    if (itsPosition >= itsSteps.size())
    {
      // A streamer whose script ran out without ending: returning nothing while
      // still OK means "come back later", which is a legitimate state
      return {};
    }

    const Step& step = itsSteps[itsPosition++];
    itsElapsed += step.took;  // Producing it "took" this long
    setStatus(step.status);
    return step.data;
  }

  std::size_t calls() const { return itsCalls; }

  //! Where the scripted clock stands, which is only moved by producing pieces
  std::chrono::milliseconds elapsed() const { return itsElapsed; }

 private:
  std::vector<Step> itsSteps;
  std::size_t itsPosition = 0;
  std::size_t itsCalls = 0;
  std::chrono::milliseconds itsElapsed{0};
};

// ----------------------------------------------------------------------
/*!
 * \brief A collector whose clock the script moves
 *
 * The gap and hold limits are about time, and a test that slept for it would be
 * both slow and at the mercy of the machine. This reads the time from the
 * scripted streamer instead, so "this piece took 700 ms" is a fact of the
 * fixture and the limits can be asserted exactly.
 */
// ----------------------------------------------------------------------

class TimedCoalescingStreamer : public CoalescingStreamer
{
 public:
  TimedCoalescingStreamer(std::shared_ptr<ScriptedStreamer> theStreamer,
                          std::size_t theMinChunkSize,
                          std::size_t thePieceLimit,
                          std::chrono::milliseconds theMaxGap,
                          std::chrono::milliseconds theMaxHold)
      : CoalescingStreamer(theStreamer, theMinChunkSize, thePieceLimit, theMaxGap, theMaxHold),
        itsScript(std::move(theStreamer))
  {
  }

 protected:
  TimePoint now() const override { return TimePoint{} + itsScript->elapsed(); }

 private:
  std::shared_ptr<ScriptedStreamer> itsScript;
};

using Step = ScriptedStreamer::Step;

std::string repeated(char c, std::size_t n)
{
  return std::string(n, c);
}

// ----------------------------------------------------------------------
/*!
 * \brief Pieces are collected up to the byte threshold, in order
 */
// ----------------------------------------------------------------------

void collects_up_to_the_threshold()
{
  auto inner = std::make_shared<ScriptedStreamer>(std::vector<Step>{
      {repeated('a', 10)}, {repeated('b', 10)}, {repeated('c', 10)}, {repeated('d', 10)}});

  CoalescingStreamer streamer(inner, 25, 100);

  const std::string first = streamer.getChunk();
  if (first != repeated('a', 10) + repeated('b', 10) + repeated('c', 10))
    TEST_FAILED("Expected three pieces collected into one 30 byte chunk, got " +
                std::to_string(first.size()) + " bytes: " + first);

  if (inner->calls() != 3)
    TEST_FAILED("Expected the wrapped streamer to be asked three times, was asked " +
                std::to_string(inner->calls()));

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief A piece already over the threshold is passed straight on
 */
// ----------------------------------------------------------------------

void a_large_piece_is_not_held()
{
  auto inner = std::make_shared<ScriptedStreamer>(
      std::vector<Step>{{repeated('a', 5000)}, {repeated('b', 5000)}});

  CoalescingStreamer streamer(inner, 1024, 100);

  if (streamer.getChunk() != repeated('a', 5000))
    TEST_FAILED("A piece over the threshold must be returned on its own");
  if (inner->calls() != 1)
    TEST_FAILED("A piece over the threshold must not cost a second call");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief The piece limit bounds collecting when the pieces are tiny
 */
// ----------------------------------------------------------------------

void the_piece_limit_bounds_collecting()
{
  std::vector<Step> steps;
  for (int i = 0; i < 20; i++)
    steps.push_back({repeated('x', 4)});

  auto inner = std::make_shared<ScriptedStreamer>(steps);

  // 4 byte pieces against a 1 MB threshold: only the piece limit can stop this
  CoalescingStreamer streamer(inner, 1048576, 5);

  if (streamer.getChunk() != repeated('x', 20))
    TEST_FAILED("Expected five 4 byte pieces");
  if (inner->calls() != 5)
    TEST_FAILED("Expected exactly five calls, got " + std::to_string(inner->calls()));

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief A producer with nothing ready yet is flushed, not waited for
 *
 * The one thing collecting must never do. An empty piece with an OK status is
 * how a streamer says "come back later", and holding what has been collected
 * until the threshold is reached would turn every slow producer into a stall.
 */
// ----------------------------------------------------------------------

void nothing_ready_yet_flushes()
{
  auto inner = std::make_shared<ScriptedStreamer>(
      std::vector<Step>{{repeated('a', 10)}, {""}, {repeated('b', 10)}});

  CoalescingStreamer streamer(inner, 1000, 100);

  if (streamer.getChunk() != repeated('a', 10))
    TEST_FAILED("Collecting must stop at a streamer that has nothing ready yet");
  if (inner->calls() != 2)
    TEST_FAILED("Expected two calls (one piece, one empty), got " +
                std::to_string(inner->calls()));

  // And the wrapper stays OK, so the server asks again
  if (streamer.getStatus() != ContentStreamer::StreamerStatus::OK)
    TEST_FAILED("An unfinished stream must keep an OK status");

  if (streamer.getChunk() != repeated('b', 10))
    TEST_FAILED("The next call must continue where the previous one stopped");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief An empty chunk is returned when the producer has nothing at all
 */
// ----------------------------------------------------------------------

void nothing_at_all_returns_nothing()
{
  auto inner = std::make_shared<ScriptedStreamer>(std::vector<Step>{{""}});

  CoalescingStreamer streamer(inner, 1000, 100);

  if (!streamer.getChunk().empty())
    TEST_FAILED("A streamer with nothing ready must not invent content");
  if (streamer.getStatus() != ContentStreamer::StreamerStatus::OK)
    TEST_FAILED("A streamer with nothing ready has not ended");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief The last bytes are kept when they arrive with EXIT_OK
 *
 * Which is how nearly every streamer ends: the call that discovers the end of
 * the data also returns what it collected on the way there. Dropping those
 * bytes would truncate the body while framing it as complete.
 */
// ----------------------------------------------------------------------

void the_tail_survives_a_terminal_status()
{
  auto inner = std::make_shared<ScriptedStreamer>(std::vector<Step>{
      {repeated('a', 10)},
      {repeated('b', 10), ContentStreamer::StreamerStatus::EXIT_OK}});

  CoalescingStreamer streamer(inner, 1000, 100);

  if (streamer.getChunk() != repeated('a', 10) + repeated('b', 10))
    TEST_FAILED("Bytes returned together with EXIT_OK must be part of the chunk");
  if (streamer.getStatus() != ContentStreamer::StreamerStatus::EXIT_OK)
    TEST_FAILED("EXIT_OK must be passed on");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief An end with no bytes left ends the stream without inventing a chunk
 */
// ----------------------------------------------------------------------

void an_empty_end_ends_the_stream()
{
  auto inner = std::make_shared<ScriptedStreamer>(std::vector<Step>{
      {repeated('a', 10)}, {"", ContentStreamer::StreamerStatus::EXIT_OK}});

  CoalescingStreamer streamer(inner, 1000, 100);

  if (streamer.getChunk() != repeated('a', 10))
    TEST_FAILED("The collected bytes must come back with the end of the stream");
  if (streamer.getStatus() != ContentStreamer::StreamerStatus::EXIT_OK)
    TEST_FAILED("EXIT_OK must be passed on");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief A stream that has already ended is not asked again
 */
// ----------------------------------------------------------------------

void a_finished_stream_is_not_asked_again()
{
  auto inner = std::make_shared<ScriptedStreamer>(
      std::vector<Step>{{repeated('a', 10), ContentStreamer::StreamerStatus::EXIT_OK}});

  CoalescingStreamer streamer(inner, 1000, 100);

  if (streamer.getChunk() != repeated('a', 10))
    TEST_FAILED("The only piece must be returned");

  const std::size_t calls = inner->calls();

  if (!streamer.getChunk().empty())
    TEST_FAILED("A finished stream has no more content");
  if (inner->calls() != calls)
    TEST_FAILED("A finished streamer must not be asked for more");
  if (streamer.getStatus() != ContentStreamer::StreamerStatus::EXIT_OK)
    TEST_FAILED("The end of the stream must stay reported");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief An error is passed on, with the bytes already handed over
 *
 * The body is broken either way and the server leaves such a response
 * unterminated, so discarding what the producer had already given is no safer -
 * only lossier.
 */
// ----------------------------------------------------------------------

void an_error_is_passed_on()
{
  auto inner = std::make_shared<ScriptedStreamer>(std::vector<Step>{
      {repeated('a', 10)}, {"", ContentStreamer::StreamerStatus::EXIT_ERROR}});

  CoalescingStreamer streamer(inner, 1000, 100);

  if (streamer.getChunk() != repeated('a', 10))
    TEST_FAILED("Bytes collected before the failure must not be thrown away");
  if (streamer.getStatus() != ContentStreamer::StreamerStatus::EXIT_ERROR)
    TEST_FAILED("EXIT_ERROR must be passed on, or a truncated body is framed as complete");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief Either limit at zero means no collecting at all
 */
// ----------------------------------------------------------------------

void zero_limits_disable_collecting()
{
  auto inner = std::make_shared<ScriptedStreamer>(
      std::vector<Step>{{repeated('a', 10)}, {repeated('b', 10)}});

  CoalescingStreamer no_bytes(inner, 0, 100);

  if (no_bytes.getChunk() != repeated('a', 10))
    TEST_FAILED("A zero byte threshold must pass each piece straight on");
  if (inner->calls() != 1)
    TEST_FAILED("A zero byte threshold must cost one call per chunk");

  auto other = std::make_shared<ScriptedStreamer>(
      std::vector<Step>{{repeated('a', 10)}, {repeated('b', 10)}});

  CoalescingStreamer no_pieces(other, 1000, 0);

  if (no_pieces.getChunk() != repeated('a', 10))
    TEST_FAILED("A zero piece limit must pass each piece straight on");
  if (other->calls() != 1)
    TEST_FAILED("A zero piece limit must cost one call per chunk");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief A piece that was slow to arrive is the last one collected
 *
 * Waiting for another piece from a producer that just spent longer than the gap
 * limit would hold what is in hand for as long again, and the framing collecting
 * saves is noise next to that much production time.
 */
// ----------------------------------------------------------------------

void a_slow_piece_ends_the_collecting()
{
  auto inner = std::make_shared<ScriptedStreamer>(
      std::vector<Step>{{repeated('a', 10), ContentStreamer::StreamerStatus::OK, 10ms},
                        {repeated('b', 10), ContentStreamer::StreamerStatus::OK, 700ms},
                        {repeated('c', 10), ContentStreamer::StreamerStatus::OK, 10ms}});

  TimedCoalescingStreamer streamer(inner, 1000, 100, 500ms, 5000ms);

  if (streamer.getChunk() != repeated('a', 10) + repeated('b', 10))
    TEST_FAILED("Collecting must stop at the piece that took longer than the gap limit");
  if (inner->calls() != 2)
    TEST_FAILED("Expected two calls, got " + std::to_string(inner->calls()));

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief A slow first piece is sent on its own
 *
 * Which is what answers the awkward case: a producer that takes a long time to
 * make its first piece. The hold limit cannot help there - nothing is being
 * held yet - but that first piece arrived through a gap like any other, so it
 * goes out as it is instead of becoming the start of a chunk nobody sees.
 */
// ----------------------------------------------------------------------

void a_slow_first_piece_is_not_held()
{
  auto inner = std::make_shared<ScriptedStreamer>(
      std::vector<Step>{{repeated('a', 10), ContentStreamer::StreamerStatus::OK, 3000ms},
                        {repeated('b', 10), ContentStreamer::StreamerStatus::OK, 1ms}});

  TimedCoalescingStreamer streamer(inner, 1000, 100, 500ms, 1000ms);

  if (streamer.getChunk() != repeated('a', 10))
    TEST_FAILED("A first piece that took minutes to make must not wait for a second");
  if (inner->calls() != 1)
    TEST_FAILED("Expected one call, got " + std::to_string(inner->calls()));

  // And the producer is not written off for it: the next chunk collects normally
  if (streamer.getChunk() != repeated('b', 10) + std::string())
    TEST_FAILED("The next chunk must be collected on its own merits");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief Collected bytes do not wait longer than the hold limit
 *
 * Pieces quick enough that the gap limit never fires, but enough of them that
 * what was collected first would otherwise sit unsent for a long time. The
 * client sees progress, and a proxy counting idle seconds sees traffic.
 */
// ----------------------------------------------------------------------

void collected_bytes_are_not_held_too_long()
{
  std::vector<Step> steps;
  for (int i = 0; i < 50; i++)
    steps.push_back({repeated('x', 10), ContentStreamer::StreamerStatus::OK, 100ms});

  auto inner = std::make_shared<ScriptedStreamer>(steps);

  // 100 ms a piece: never a gap of 500 ms, so only the hold limit can stop this.
  // The limit is on how long the bytes in hand have waited, and they arrived with
  // the first piece 100 ms in, so it is the eleventh piece that finds them a full
  // second old.
  TimedCoalescingStreamer streamer(inner, 100000, 1000, 500ms, 1000ms);

  const std::string first = streamer.getChunk();

  if (first != repeated('x', 110))
    TEST_FAILED("Expected the eleven pieces a second of waiting makes, got " +
                std::to_string(first.size()) + " bytes");
  if (inner->calls() != 11)
    TEST_FAILED("Expected eleven calls, got " + std::to_string(inner->calls()));

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief Zero disables a time limit
 */
// ----------------------------------------------------------------------

void zero_disables_the_time_limits()
{
  std::vector<Step> steps;
  for (int i = 0; i < 5; i++)
    steps.push_back({repeated('x', 10), ContentStreamer::StreamerStatus::OK, 9000ms});
  steps.push_back({"", ContentStreamer::StreamerStatus::EXIT_OK, 0ms});

  auto inner = std::make_shared<ScriptedStreamer>(steps);

  // Nine seconds a piece, and neither limit applied: only the count stops this
  TimedCoalescingStreamer streamer(inner, 100000, 5, 0ms, 0ms);

  if (streamer.getChunk() != repeated('x', 50))
    TEST_FAILED("With both time limits at zero only size and count may stop collecting");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief The default clock is wired to the limits
 *
 * The cases above decide what time it is, which tests the limits but not that a
 * collector left to itself reads a clock at all. This one really does spend the
 * time, which is why it is the only one that does and why it spends little.
 */
// ----------------------------------------------------------------------

class SleepyStreamer : public ContentStreamer
{
 public:
  SleepyStreamer(std::size_t thePieces, std::chrono::milliseconds theDelay)
      : itsPiecesLeft(thePieces), itsDelay(theDelay)
  {
  }

  std::string getChunk() override
  {
    if (itsPiecesLeft == 0)
    {
      setStatus(StreamerStatus::EXIT_OK);
      return {};
    }

    --itsPiecesLeft;
    std::this_thread::sleep_for(itsDelay);
    return std::string(10, 'z');
  }

 private:
  std::size_t itsPiecesLeft;
  std::chrono::milliseconds itsDelay;
};

void the_default_clock_is_used()
{
  // 20 ms a piece against a 10 ms gap limit: the first piece ends the chunk
  auto inner = std::make_shared<SleepyStreamer>(4, 20ms);

  CoalescingStreamer streamer(inner, 100000, 100, 10ms, 1000ms);

  if (streamer.getChunk() != repeated('z', 10))
    TEST_FAILED("A collector reading the real clock must honour the gap limit too");

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief A whole body comes through byte for byte
 *
 * A reproducible pseudo-random sequence, so the bytes are their own fixture:
 * the same generator says what should have arrived, and any piece dropped,
 * duplicated or reordered by the collecting changes the result.
 */
// ----------------------------------------------------------------------

void a_whole_body_survives_unchanged()
{
  // xorshift64*, seeded: cheap, and identical wherever it is written down
  std::uint64_t state = 20260908;
  auto next_byte = [&state]()
  {
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return static_cast<char>((state * 0x2545F4914F6CDD1DULL) >> 56);
  };

  std::string expected;
  std::vector<Step> steps;
  for (int i = 0; i < 500; i++)
  {
    // Pieces of wildly different sizes, including empty ones
    const std::size_t length = (i % 7 == 0) ? 0 : (1 + (i * 37) % 300);
    std::string piece;
    for (std::size_t j = 0; j < length; j++)
      piece += next_byte();

    expected += piece;
    steps.push_back({piece});
  }
  steps.push_back({"", ContentStreamer::StreamerStatus::EXIT_OK});

  auto inner = std::make_shared<ScriptedStreamer>(steps);
  CoalescingStreamer streamer(inner, 4096, 128);

  std::string received;
  std::size_t chunks = 0;
  while (streamer.getStatus() == ContentStreamer::StreamerStatus::OK)
  {
    received += streamer.getChunk();
    chunks++;

    if (chunks > steps.size())
      TEST_FAILED("The wrapper never finished the stream");
  }

  if (streamer.getStatus() != ContentStreamer::StreamerStatus::EXIT_OK)
    TEST_FAILED("The stream must end in EXIT_OK");
  if (received != expected)
    TEST_FAILED("Body came through as " + std::to_string(received.size()) + " bytes, expected " +
                std::to_string(expected.size()));
  if (chunks >= 500)
    TEST_FAILED("Collecting produced " + std::to_string(chunks) +
                " chunks from 500 pieces, which is no collecting at all");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

class tests : public tframe::tests
{
  virtual const char* error_message_prefix() const { return "\n\t"; }

  void test()
  {
    TEST(collects_up_to_the_threshold);
    TEST(a_large_piece_is_not_held);
    TEST(the_piece_limit_bounds_collecting);
    TEST(nothing_ready_yet_flushes);
    TEST(nothing_at_all_returns_nothing);
    TEST(the_tail_survives_a_terminal_status);
    TEST(an_empty_end_ends_the_stream);
    TEST(a_finished_stream_is_not_asked_again);
    TEST(an_error_is_passed_on);
    TEST(zero_limits_disable_collecting);
    TEST(a_slow_piece_ends_the_collecting);
    TEST(a_slow_first_piece_is_not_held);
    TEST(collected_bytes_are_not_held_too_long);
    TEST(zero_disables_the_time_limits);
    TEST(the_default_clock_is_used);
    TEST(a_whole_body_survives_unchanged);
  }
};

}  // namespace CoalescingStreamerTest

int main()
{
  std::cout << std::endl
            << "CoalescingStreamer tester" << std::endl
            << "=========================" << std::endl;
  CoalescingStreamerTest::tests t;
  return t.run();
}
