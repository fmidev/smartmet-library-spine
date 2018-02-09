// ======================================================================
/*!
 * \brief Interface of class SmartMetEngine
 *
 * This is the base class for all SmartMet server engines.
 * Engines must inherit from this class and then implement
 * all the declared methods.
 */
// ======================================================================

#pragma once

#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>

#include <string>

namespace SmartMet
{
namespace Spine
{
/// Forward declaration
class Reactor;

// The type definitions of the class factories
// *** Do not touch these unless you know exactly what you are doing ***

class SmartMetEngine : private boost::noncopyable
{
  friend class SmartMet::Spine::Reactor;

 public:
  /// Constructor
  SmartMetEngine() = default;

  /// Virtual destructor declaration (otherwise runtime relocation will fail)
  virtual ~SmartMetEngine();

  virtual void setShutdownRequestedFlag();
  virtual void shutdownEngine();

 protected:
  /// This function contains the engine construction
  virtual void init() = 0;

  /// This function request the engine to close all its activities.
  virtual void shutdown() = 0;
  virtual void shutdownRequestFlagSet();

  bool itsShutdownRequested = false;
  Reactor* itsReactor = nullptr;

 private:
  /// This function is used by Reactor getSingleton - method to wait until the engine is constructed
  void wait();

  /// This function is used by the Reactor to initialize the engine
  void construct(const std::string& engineName, Reactor* reactor);

  bool isReady = false;
  std::string itsName;
  boost::mutex itsInitMutex;
  boost::condition_variable itsCond;
};

}  // namespace Spine
}  // namespace SmartMet
