// ======================================================================
/*!
 * \brief Interface of class DynamicPlugin
 *
 * DynamicPlugin is a wrapper around individual SmartMet plugins
 */
// ======================================================================

#pragma once

#include "SmartMetPlugin.h"
#include <string>

#include <boost/thread.hpp>

namespace SmartMet
{
namespace Spine
{
class Reactor;

class DynamicPlugin
{
 public:
  // Constructor
  DynamicPlugin(const std::string& theFilename, const std::string& theConfig, Reactor& theReactor);

  // Destructor
  ~DynamicPlugin();

  // Get the filename of the plugin
  const std::string& filename() const { return itsFilename; }
  // Get the name of the plugin
  const std::string& pluginname() const { return itsPlugin->getPluginName(); }
  // Get the API version
  int apiversion() const { return itsPlugin->getRequiredAPIVersion(); }
  // Method to reload the dynamic library plugin
  void reloadPlugin();

  SmartMetPlugin* getPlugin() const { return itsPlugin; }
  // Initialize the plugin (call the init-method in a new thread)
  void initializePlugin();

  void setShutdownRequestedFlag();
  void shutdownPlugin();

 protected:
  // Filename of the dynamic library
  const std::string itsFilename;

  // Configfile
  const std::string itsConfigFile;

  // Reference to SmartMet::Server class instance
  Reactor& itsReactorClass;

  // Pointer to instance
  SmartMetPlugin* itsPlugin;

 private:
  // Pointer to dynamic library
  void* itsHandle;

  // Create and Destroy function pointers
  plugin_create_t* plugin_create_func;
  plugin_destroy_t* plugin_destroy_func;

  // Method to open the dynamic module, resolve create/destroy symbols,
  // and create an instance of the dynamic class.
  void pluginOpen();

  // Method to call the destroy function of dynamic module,
  // and close the dynamic module.
  void pluginClose();
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
