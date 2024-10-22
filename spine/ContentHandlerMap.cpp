#include "ContentHandlerMap.h"
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <macgyver/AnsiEscapeCodes.h>
#include <iostream>
#include "ConfigTools.h"
#include "Convenience.h"
#include "HTTP.h"
#include "Reactor.h"

using SmartMet::Spine::ContentHandler;
using SmartMet::Spine::ContentHandlerMap;
using SmartMet::Spine::HandlerView;
using SmartMet::Spine::Reactor;

ContentHandlerMap::ContentHandlerMap(const Options& options)
try
    : itsOptions(options)
{
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}


ContentHandlerMap::~ContentHandlerMap() = default;


void ContentHandlerMap::setNoMatchHandler(ContentHandler theHandler)
try
{
  WriteLock lock(itsContentMutex);
  if (theHandler)
  {
    itsCatchNoMatchHandler.reset(new HandlerView(theHandler));
  }
  else
  {
    itsCatchNoMatchHandler.reset();
  }
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}


bool ContentHandlerMap::addContentHandler(SmartMetPlugin* thePlugin,
                                          const std::string& theUri,
                                          const ContentHandler& theHandler,
                                          bool handlesUriPrefix,
                                          bool isPrivate)
try
{
  WriteLock lock(itsContentMutex);
  WriteLock lock2(itsLoggingMutex);

  const std::string pluginName = thePlugin->getPluginName();

  auto itsFilter = itsIPFilters.find(Fmi::ascii_tolower_copy(pluginName));
  std::shared_ptr<IPFilter::IPFilter> filter;
  if (itsFilter != itsIPFilters.end())
  {
    filter = itsFilter->second;
  }

  std::cout << Spine::log_time_str() << ANSI_BOLD_ON << ANSI_FG_GREEN << " Registered "
            << (isPrivate ? "private " : "") << "URI " << theUri << " for plugin "
            << thePlugin->getPluginName() << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << std::endl;

  std::unique_ptr<HandlerView> handler(new HandlerView(theHandler,
                                                       filter,
                                                       thePlugin,
                                                       theUri,
                                                       itsLoggingEnabled,
                                                       isPrivate,
                                                       itsOptions.accesslogdir));

  if (handlesUriPrefix)
  {
    itsUriPrefixes.insert(theUri);
  }

  itsHandlers[theUri] = std::move(handler);

  return true;
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}

std::size_t ContentHandlerMap::removeContentHandlers(SmartMetPlugin* thePlugin)
try
{
  WriteLock lock(itsContentMutex);

  std::size_t count = 0;
  for (auto it = itsHandlers.begin(); it != itsHandlers.end();)
  {
    auto curr = it++;
    if (curr->second and curr->second->usesPlugin(thePlugin))
    {
      const std::string uri = curr->first;
      const std::string name = curr->second->getPluginName();
      itsHandlers.erase(curr);
      itsUriPrefixes.erase(uri);
      count++;
      std::cout << Spine::log_time_str() << ANSI_BOLD_ON << ANSI_FG_GREEN << " Removed URI " << uri
                << " handled by plugin " << name << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << std::endl;
    }
  }
  return count;
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}

HandlerView* ContentHandlerMap::getHandlerView(const HTTP::Request& theRequest)
try
{
  ReadLock lock(itsContentMutex);

  bool remapped = false;
  std::string resource = theRequest.getResource();

  for (const auto& item : itsUriPrefixes)
  {
    std::size_t len = item.length();
    if (resource.substr(0, len) == item && (resource.length() == len || resource[len] == '/'))
    {
      remapped = true;
      resource = item;
      break;
    }
  }

  // Try to find a content handler
  auto it = itsHandlers.find(resource);
  if (it == itsHandlers.end())
  {
    if (remapped)
    {
      // Should never happen
      throw Fmi::Exception(BCP,
                          "[INTERNAL ERROR] URI remapping defined, but"
                          " handler not found for " +
                           resource);
    }

    // No specific match found, decide what we should do
    // Return with true, as this was catched by external handler (or nullptr if not provided)
    return &*itsCatchNoMatchHandler;
  }

  return &*it->second;
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}


void ContentHandlerMap::addIPFilters(const std::string& pluginName, const std::vector<std::string>& filterTokens)
try
{
  std::shared_ptr<IPFilter::IPFilter> theFilter;
  try
  {
    theFilter.reset(new IPFilter::IPFilter(filterTokens));
    std::cout << "IP Filter registered for plugin: " << pluginName << std::endl;
  }
  catch (std::runtime_error& err)
  {
    // No IP filter for this plugin
    std::cout << "No IP filter for plugin: " << pluginName << ". Reason: " << err.what()
              << std::endl;
  }

  auto inserted = itsIPFilters.insert(std::make_pair(pluginName, theFilter));
  if (!inserted.second)
  {
    // Plugin name is not unique
    std::cout << "No IP filter for plugin: " << pluginName << ". Reason: plugin name not unique"
              << std::endl;
  }
}
catch (...)
{
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
}


SmartMet::Spine::URIMap ContentHandlerMap::getURIMap() const
try
{
  ReadLock lock(itsContentMutex);
  SmartMet::Spine::URIMap theMap;

  for (const auto& handlerPair : itsHandlers)
  {
    // Getting plugin names during shutdown may throw due to a call to a pure virtual method.
    // This mitigates the problem, but does not solve it. The shutdown flag should be
    // locked for the duration of this loop.
    if (Reactor::isShuttingDown())
      return {};

    if (not handlerPair.second->isPrivate())
    {
      theMap.insert(std::make_pair(handlerPair.first, handlerPair.second->getPluginName()));
    }
  }

  return theMap;
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}

SmartMet::Spine::AccessLogStruct ContentHandlerMap::getLoggedRequests(const std::string& thePlugin) const
try
{
    if (itsLoggingEnabled)
    {
      std::string pluginNameInLowerCase = Fmi::ascii_tolower_copy(thePlugin);
      LoggedRequests requests;
      ReadLock lock(itsContentMutex);
      for (const auto& handler : itsHandlers)
      {
        if (pluginNameInLowerCase == "all" ||
            pluginNameInLowerCase == Fmi::ascii_tolower_copy(handler.second->getPluginName()))
          requests.insert(std::make_pair(handler.first, handler.second->getLoggedRequests()));
      }
      return std::make_tuple(true, requests, itsLogLastCleaned);
    }

    return std::make_tuple(false, LoggedRequests(), Fmi::DateTime());
}
catch(const std::exception& e)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}

std::optional<std::string> ContentHandlerMap::getPluginName(const std::string& uri) const
{
  ReadLock lock(itsContentMutex);
  auto it = itsHandlers.find(uri);
  if (it != itsHandlers.end())
    return it->second->getPluginName();
  return std::nullopt;
}


void ContentHandlerMap::dumpURIs(std::ostream& output) const
{
  ReadLock lock(itsContentMutex);
  for (const auto& item : itsHandlers)
  {
      output << item.first << " --> " << item.second->getPluginName() << std::endl;
  }
}

void ContentHandlerMap::setLogging(bool loggingEnabled)
try
{
  WriteLock lock(itsLoggingMutex);

  // Check for no change in status
  if (itsLoggingEnabled == loggingEnabled)
    return;

  itsLoggingEnabled = loggingEnabled;

  if (itsLoggingEnabled)
  {
    // See if cleaner thread is running for some reason
    if (itsLogCleanerThread.get() != nullptr && itsLogCleanerThread->joinable())
    {
      // Kill any remaining thread
      itsLogCleanerThread->interrupt();
      itsLogCleanerThread->join();
    }

    // Launch log cleaner thread
    itsLogCleanerThread.reset(new boost::thread(boost::bind(&ContentHandlerMap::cleanLog, this)));

    // Set log cleanup time
    itsLogLastCleaned = Fmi::SecondClock::local_time();
  }
  else
  {
    // Status set to false, make the transition true->false
    // Erase log, stop cleaning thread
    itsLogCleanerThread->interrupt();
    itsLogCleanerThread->join();
  }

  // Set logging status for ALL plugins
  WriteLock lock2(itsContentMutex);
  for (auto& handlerPair : itsHandlers)
  {
    handlerPair.second->setLogging(itsLoggingEnabled);
  }
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}

/* [[noreturn]] */ void ContentHandlerMap::cleanLog()
try
{
  // This function must be called as an argument to the cleaner thread

  auto maxAge = Fmi::Hours(24);  // Here we give the maximum log time span, 24 Fmi::SecondClock

  while (!Reactor::isShuttingDown())
  {
    // Sleep for some time
    boost::this_thread::sleep_for(boost::chrono::seconds(5));

    auto firstValidTime = Fmi::SecondClock::local_time() - maxAge;

    ReadLock lock(itsLoggingMutex);
    for (auto& handlerPair : itsHandlers)
    {
      if (Reactor::isShuttingDown())
        return;

      handlerPair.second->cleanLog(firstValidTime);
      handlerPair.second->flushLog();
    }

    if (itsLogLastCleaned < firstValidTime)
    {
      itsLogLastCleaned = firstValidTime;
    }
  }
}
catch (...)
{
  // FIXME: should we really catch all exceptions here?
  //        it will cause SIGABRT
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}

bool ContentHandlerMap::isURIPrefix(const std::string& uri) const
{
  return itsUriPrefixes.count(uri) > 0;
}
