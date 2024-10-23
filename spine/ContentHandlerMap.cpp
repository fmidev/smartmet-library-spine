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

void ContentHandlerMap::setAdminUri(const std::string& theUri)
{
  WriteLock lock(itsContentMutex);
  itsAdminUri = theUri;
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

  const auto result = itsHandlers.emplace(theUri, std::move(handler));
  if (not result.second)
  {
    const std::string name = result.first->second->getPluginName();
    std::ostringstream msg;
    msg << "Attempt to register URI " << theUri << " for plugin " << pluginName
        << " failed: URI already registered for plugin " << name;
    throw Fmi::Exception(BCP, msg.str());
  }

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

  for (auto it1 = itsAdminRequestHandlers.begin(); it1 != itsAdminRequestHandlers.end(); )
  {
    int erased = 0;
    auto curr = it1++;
    const std::string what = curr->first;
    for (auto it2 = curr->second.begin(); it2 == curr->second.end(); )
    {
      auto item = it2++;
      const AdminRequestTarget& target = item->first;
      const SmartMetPlugin* plugin = *std::get_if<SmartMetPlugin*>(&target);
      if (plugin and plugin == thePlugin)
      {
        curr->second.erase(item);
        std::cout << Spine::log_time_str() << ANSI_BOLD_ON << ANSI_FG_GREEN
                  << " Removed admin handler (what=')" << what
                  << "' for plugin " << thePlugin->getPluginName()
                  << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << std::endl;
        if (itsUniqueAdminRequests.count(what))
        {
          itsUniqueAdminRequests.erase(what);
        }
        erased++;
      }

      if (erased && curr->second.empty())
      {
        itsAdminRequestHandlers.erase(curr);
        count += erased;
      }
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

bool ContentHandlerMap::addAdminRequestHandler(AdminRequestTarget target,
                                              const std::string& what,
                                              bool requiresAuthentication,
                                              const ContentHandler& theHandler,
                                              const std::string& description,
                                              bool unique)
try
{
  WriteLock lock(itsContentMutex);

  std::shared_ptr<IPFilter::IPFilter> filter;

  // FIXME: get IP filter for admin requests

  std::cout << Spine::log_time_str() << ANSI_BOLD_ON << ANSI_FG_GREEN
            << " Registered admin request handler for " << targetName(target)
            << ' ' << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << " (what='" << what << "')"
            << std::endl;

  auto handler = std::make_unique<AdminRequestInfo>();
  handler->what = what;
  handler->target = target;
  handler->requiresAuthentication = requiresAuthentication;
  handler->handler = theHandler;
  handler->description = description;

  // Try adding plugin entry for admin requests. It does not matter whether
  // plugin is already present as pos1.first is always valid is always valid
  const auto pos1 = itsAdminRequestHandlers.emplace(what,
    std::map<AdminRequestTarget, std::unique_ptr<AdminRequestInfo>>());

  // Should be new entry if unique==true
  if (pos1.second)
  {
    if (itsUniqueAdminRequests.count(what))
    {
      // Already defined and some earlier request required to be unique : report error
      const std::string name = targetName(target);
      std::ostringstream err;
      err << "Failed to add admin request (what='" << what << "') for " << name
          << " (already defined and required to be unique)";
      throw Fmi::Exception(BCP, err.str());
    }
  }
  else
  {
    if (unique)
    {
      // Already defined and required to be unique : report error
      const std::string name = targetName(target);
      std::ostringstream err;
      err << "Failed to add admin request (what='" << what << "') for plugin '"
          << name << "' (already defined and required to be unique)";
      throw Fmi::Exception(BCP, err.str());
    }
  }

  auto& dest_map = pos1.first->second;

  const auto result = dest_map.emplace(target, std::move(handler));
  if (not result.second)
  {
    const std::string name = targetName(target);
    std::ostringstream err;
    err << "Failed to add admin request (what='" << what << "') for plugin '"
        << name << "' (already defined)";
    throw Fmi::Exception(BCP, err.str());
  }

  if (unique)
  {
    itsUniqueAdminRequests.insert(what);
  }

  return true;
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}

bool ContentHandlerMap::removeAdminRequestHandler(AdminRequestTarget target,
                                                 const std::string& what)
try
{
  WriteLock lock(itsContentMutex);

  const auto it1 = itsAdminRequestHandlers.find(what);
  if (it1 != itsAdminRequestHandlers.end())
  {
    auto& dest_map = it1->second;
    const auto it2 = dest_map.find(target);
    if (it2 != dest_map.end())
    {
      dest_map.erase(it2);
      if (itsUniqueAdminRequests.count(what))
      {
        itsUniqueAdminRequests.erase(what);
      }
      return true;
    }
  }

  std::cout << Spine::log_time_str() << ANSI_BOLD_ON << ANSI_FG_RED
            << " Admin request handler not found for " << targetName(target)
            << ' ' << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << " (what='" << what << "')"
            << std::endl;
  return false;
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}


bool ContentHandlerMap::executeAdminRequest(
    const HTTP::Request& theRequest,
    HTTP::Response& theResponse,
    std::function<bool(const HTTP::Request&)> authCallback)
{
  try
  {
    ReadLock lock(itsContentMutex);

    const auto what = theRequest.getParameter("what");
    if (not what)
    {
      theResponse.setStatus(HTTP::Status::bad_request);
      theResponse.setContent("Missing 'what' parameter\n");
      return false;
    }

    const auto it = itsAdminRequestHandlers.find(*what);
    if (it == itsAdminRequestHandlers.end())
    {
      theResponse.setStatus(HTTP::Status::not_found);
      theResponse.setContent("Unknown admin request: " + *what);
      return false;
    }

    // Check whether any of the handlers require authentication
    // Assume that all handlers for the same 'what' require the same authentication
    bool requiresAuth = false;
    for (const auto& item : it->second)
    {
      if (item.second->requiresAuthentication)
      {
        requiresAuth = true;
        break;
      }
    };

    if (requiresAuth)
    {
      if (authCallback)
      {
        if (!authCallback(theRequest))
        {
          theResponse.setStatus(HTTP::Status::unauthorized);
          theResponse.setContent("Authentication required");
          return false;
        }
      } else {
        theResponse.setStatus(HTTP::Status::forbidden);
        theResponse.setContent("Authentication required but callback not provided");
        return false;
      }
    }

    Reactor* reactor = dynamic_cast<Reactor*>(this);
    if (!reactor)
    {
      throw Fmi::Exception(BCP, "INTERNAL ERROR: Reactor not available");
    }

    for (const auto& item : it->second)
    {
      // FIXME: what to do if one handler of several throws an exception?
      const auto& handler = item.second->handler;
      handler(*reactor, theRequest, theResponse);
    }

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::unique_ptr<SmartMet::Spine::Table> ContentHandlerMap::getAdminRequests() const
{
  int y = 0;
  auto result = std::make_unique<SmartMet::Spine::Table>();
  ReadLock lock(itsContentMutex);
  for (const auto& item1 : itsAdminRequestHandlers)
  {
    const std::string& what = item1.first;
    for (const auto& item2 : item1.second)
    {
      const std::string& plugin_name = targetName(item2.second->target);
      const std::string& description = item2.second->description;
      const std::string authInfo = item2.second->requiresAuthentication ? "yes" : "no";
      const std::string unique = itsUniqueAdminRequests.count(what) ? "yes" : "no";
      result->set(0, y, what);
      result->set(1, y, plugin_name);
      result->set(2, y, authInfo);
      result->set(3, y, unique);
      result->set(4, y, description);
      y++;
    }
  }
  return result;
}

std::string ContentHandlerMap::targetName(
    const ContentHandlerMap::AdminRequestTarget& target)
{
  if (auto* ptr = std::get_if<SmartMetPlugin *>(&target))
  {
    return (*ptr)->getPluginName() + " plugin";
  }

  if (auto* ptr = std::get_if<SmartMetEngine *>(&target))
  {
    return (*ptr)->getEngineName() + " engine";
  }

  throw Fmi::Exception(BCP, "INTERNAL ERROR: Unknown admin request target");

}
