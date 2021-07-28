// ======================================================================
/*!
 * \brief Interface of class SmartMetPlugin
 *
 * This is the base class for all SmartMet server plugins.
 * Plugins should inherit from this class and then implement
 * all the declared methods.
 */
// ======================================================================

#pragma once
#include "HTTP.h"
#include <atomic>
#include <string>

// The type definitions of the class factories
// *** Do not touch these unless you know exactly what you are doing ***

namespace SmartMet
{
namespace Spine
{
class Reactor;
}
}  // namespace SmartMet

class SmartMetPlugin
{
 public:
  SmartMetPlugin();

  // Virtual destructor declaration (otherwise runtime relocation will fail)
  virtual ~SmartMetPlugin() = default;

  // Method to get name and description of this dynamic module
  virtual const std::string &getPluginName() const = 0;

  // Method to get the Braisntorm API version required by this dynamic module
  virtual int getRequiredAPIVersion() const = 0;

  // Method to determine if incoming query is fast or slow
  virtual bool queryIsFast(const SmartMet::Spine::HTTP::Request &theRequest) const;

  // Plugin initialization
  void initPlugin();
  bool isInitActive();

  // Plugin shutdown
  void shutdownPlugin();
  bool isShutdownRequested();
  void setShutdownRequestedFlag();

  // Method to process incoming requests
  void callRequestHandler(SmartMet::Spine::Reactor &theReactor,
                          const SmartMet::Spine::HTTP::Request &theRequest,
                          SmartMet::Spine::HTTP::Response &theResponse);

 protected:
  // Each plugin should implement the following methods
  virtual void init() = 0;
  virtual void shutdown() = 0;
  virtual void requestHandler(SmartMet::Spine::Reactor &theReactor,
                              const SmartMet::Spine::HTTP::Request &theRequest,
                              SmartMet::Spine::HTTP::Response &theResponse) = 0;

  bool itsInitActive;
  bool itsShutdownRequested;
  std::atomic<unsigned long long> requestCounter;
  std::atomic<unsigned long long> responseCounter;
};

using plugin_create_t = SmartMetPlugin *(SmartMet::Spine::Reactor *, const char *);
using plugin_destroy_t = void(SmartMetPlugin *);

// ======================================================================
