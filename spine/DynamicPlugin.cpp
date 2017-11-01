// ======================================================================
/*!
 * \brief Implementation of class DynamicPlugin
 */
// ======================================================================

#include "DynamicPlugin.h"
#include "Convenience.h"
#include "Exception.h"
#include "Reactor.h"

#include <macgyver/AnsiEscapeCodes.h>

#include <boost/bind.hpp>
#include <boost/timer/timer.hpp>

#include <sys/types.h>
#include <iostream>
#include <signal.h>
#include <stdexcept>
#include <string>

extern "C" {
#include <dlfcn.h>
}

namespace SmartMet
{
namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

DynamicPlugin::DynamicPlugin(const std::string& theFilename,
                             const std::string& theConfig,
                             Reactor& theReactor)
    : itsFilename(theFilename),
      itsConfigFile(theConfig),
      itsReactorClass(theReactor),
      itsPlugin(NULL),
      initialized(false)
{
  try
  {
    // Call the actual module loader implementation
    pluginOpen();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void DynamicPlugin::initializePlugin()
{
  try
  {
    std::string report = (std::string(ANSI_FG_GREEN) + "Plugin [" + itsPlugin->getPluginName() +
                          "] initialized in %t sec CPU, %w sec real] \n" + ANSI_FG_DEFAULT);

    boost::timer::auto_cpu_timer timer(2, report);

    itsPlugin->initPlugin();
    initialized = true;
    itsReactorClass.pluginInitializedCallback(this);
  }
  catch (...)
  {
    SmartMet::Spine::Exception exception(BCP, "Operation failed!", NULL);
    if (!exception.stackTraceDisabled())
      std::cerr << exception.getStackTrace();
    else if (!exception.loggingDisabled())
      std::cerr << Spine::log_time_str() + " Error: " + exception.what() << std::endl;

    kill(getpid(), SIGKILL);  // If we use exit() we might get a core dump.
                              // exit(-1);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

DynamicPlugin::~DynamicPlugin()
{
  // Debug
  std::cout << "\t  + [Destructing dynamic plugin '" << itsFilename << "']" << std::endl;

  // Call the actual module destroy implementation (private method)
  pluginClose();
}

// ----------------------------------------------------------------------
/*!
 * \brief Method to reload the dynamic library
 */
// ----------------------------------------------------------------------

void DynamicPlugin::reloadPlugin()
{
  try
  {
// Debug output
#ifdef DEBUG
    std::cout << "\t  + [Reloading dynamic plugin '" << itsFilename << "']" << std::endl;
#endif

    // Close the old dynamic module
    pluginClose();

    // Load the dynamic module again
    pluginOpen();

// Debug output
#ifdef DEBUG
    std::cout << "\t   + Dynamic plugin '" << itsFilename << "' reloaded" << std::endl;
#endif
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Method to open the dynamic module
 *
 * Resolve create/destroy symbols, and create an instance of
 * the dynamic class.
 */
// ----------------------------------------------------------------------

void DynamicPlugin::pluginOpen()
{
  try
  {
    // Open the dynamic library specified by private data
    // member module_filename

    if (itsReactorClass.lazyLinking())
      itsHandle = dlopen(itsFilename.c_str(), RTLD_LAZY);
    else
      itsHandle = dlopen(itsFilename.c_str(), RTLD_NOW);

    if (itsHandle == 0)
    {
      // Error occurred while opening the dynamic library
      throw SmartMet::Spine::Exception(
          BCP, "Unable to load dynamic library plugin: " + std::string(dlerror()));
    }

    // Load the symbols (pointers to functions in dynamic library)

    plugin_create_func = reinterpret_cast<plugin_create_t*>(dlsym(itsHandle, "create"));

    plugin_destroy_func = reinterpret_cast<plugin_destroy_t*>(dlsym(itsHandle, "destroy"));

    // Check that pointers to function were loaded succesfully
    if (plugin_create_func == 0 || plugin_destroy_func == 0)
    {
      throw SmartMet::Spine::Exception(BCP, "Cannot load symbols: " + std::string(dlerror()));
    }

    // Create an instance of the class using the pointer to "create" function

    itsPlugin = plugin_create_func(&itsReactorClass, itsConfigFile.c_str());

    if (itsPlugin == 0)
    {
      throw SmartMet::Spine::Exception(BCP, "Unable to create a new instance of plugin class");
    }

    // Verify that the Plugin and Reactor API versions match.

    if (itsPlugin->getRequiredAPIVersion() != itsReactorClass.getRequiredAPIVersion())
    {
      throw SmartMet::Spine::Exception(BCP, "Plugin and Server SmartMet API Version mismatch.");
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Destroy dynamic module
 *
 * Method to call the destroy function of dynamic module,
 * and close the dynamic module.
 */
// ----------------------------------------------------------------------

void DynamicPlugin::pluginClose()
{
  try
  {
    // If there is an instance of this module created,
    // and pointer to destroy function is valid.

    if (itsPlugin != 0 && plugin_destroy_func != 0)
    {
      // Call the destroy function of the dynamic module.
      plugin_destroy_func(itsPlugin);

      // Close the dynamic library
      dlclose(itsHandle);

      // Reset all the pointers to zero, because these may be recycled.
      itsPlugin = 0;
      itsHandle = 0;
      plugin_create_func = 0;
      plugin_destroy_func = 0;
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void DynamicPlugin::shutdownPlugin()
{
  try
  {
    if (itsPlugin != NULL)
    {
      itsPlugin->shutdownPlugin();
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void DynamicPlugin::setShutdownRequestedFlag()
{
  try
  {
    if (itsPlugin != NULL)
    {
      itsPlugin->setShutdownRequestedFlag();
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Spine
}  // namespace SmartMet
