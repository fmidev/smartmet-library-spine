#include "ContentHandlerMap.h"
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/TimeFormatter.h>
#include <iostream>
#include <sstream>
#include "ConfigTools.h"
#include "Convenience.h"
#include "HostInfo.h"
#include "HTTP.h"
#include "HTTPAuthentication.h"
#include "Reactor.h"
#include "TableFormatterFactory.h"
#include "TableFormatterOptions.h"

using SmartMet::Spine::ContentHandler;
using SmartMet::Spine::ContentHandlerMap;
using SmartMet::Spine::HandlerView;
using SmartMet::Spine::Reactor;
using SmartMet::Spine::Table;

using namespace std::string_literals;

namespace p = std::placeholders;

namespace
{
  using namespace SmartMet::Spine;

  /**
   * @brief Format request handler target name for use in messages
   */
  std::string targetName(
    const ContentHandlerMap::HandlerTarget& target)
  {
    if (std::holds_alternative<SmartMetPlugin *>(target))
    {
      return std::get<SmartMetPlugin*>(target)->getPluginName() + " plugin";
    }
    else if (std::holds_alternative<SmartMetEngine *>(target))
    {
      return std::get<SmartMetEngine*>(target)->getEngineName() + " engine";
    }
    else if (std::holds_alternative<ContentHandlerMap::NoTarget>(target))
    {
      return "<builtin>";
    }
    else
    {
      throw Fmi::Exception(BCP, "INTERNAL ERROR: Unknown admin request target");
    }
  }

  /**
   * @brief Format string about admin request handler for use in log messages
   */
  std::string adminRequestName(ContentHandlerMap::HandlerTarget target, const std::string& what)
  {
    const bool is_builtin = std::holds_alternative<ContentHandlerMap::NoTarget>(target);
    // Format string for use in log messages
    std::ostringstream nm;
    nm << (is_builtin ? "builtin " : "") << "admin request " << what << " handler";
    if (not is_builtin)
      nm << " for " << targetName(target);
    return nm.str();
  }
}


ContentHandlerMap::ContentHandlerMap(const Options& options)
try
    : itsOptions(options)
    , itsAdminHandlerInfo(new AdminHandlerInfo(options))
{
  // Register some admin request handlers

  // FIXME: get value of itsAdminUri from options
  // FIXME: setup IP access rules for admin requests from configuration
  if (itsAdminHandlerInfo->itsAdminUri)
  {
    addPrivateContentHandler(nullptr, *itsAdminHandlerInfo->itsAdminUri,
      std::bind(&ContentHandlerMap::handleAdminRequest, this, p::_2, p::_3));
  }

  if (itsAdminHandlerInfo->itsInfoUri)
  {
    addContentHandler(nullptr, *itsAdminHandlerInfo->itsInfoUri,
      std::bind(&ContentHandlerMap::handleAdminRequest, this, p::_2, p::_3));
  }

  // Register request to list available requests
  addAdminTableRequestHandler(
    NoTarget{},
    "list",
    AdminRequestAccess::Public,
    [this](Reactor&, const HTTP::Request& request) -> std::unique_ptr<Table>
    {
      const std::string resource = request.getResource();
      const bool isInfo = itsAdminHandlerInfo->itsInfoUri && resource == *itsAdminHandlerInfo->itsInfoUri;
      return getAdminRequestsImpl(std::nullopt, isInfo);
    },
    "List all admin requests");

  addAdminTableRequestHandler(
    NoTarget{},
    "getlogging",
    AdminRequestAccess::Private,
    std::bind(&ContentHandlerMap::getLoggingRequest, this, p::_1, p::_2),
    "Get logging status");

  addAdminBoolRequestHandler(
    NoTarget{},
    "setlogging",
    AdminRequestAccess::RequiresAuthentication,
    std::bind(&ContentHandlerMap::setLoggingRequest, this, p::_1, p::_2), "Set logging status");

  addAdminTableRequestHandler(
    NoTarget{},
    "serviceinfo",
    AdminRequestAccess::Public,
    std::bind(&ContentHandlerMap::serviceInfoRequest, this, p::_1, p::_2), "Get info about available services");
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}


ContentHandlerMap::~ContentHandlerMap()
{
  itsAdminHandlerInfo.reset();

  int count = 0;
  for (const auto& handler : itsHandlers)
  {
    std::cout << "Remaining content handler: " << handler.first << std::endl;
    count++;
  }

  for (const auto& item1 : itsAdminRequestHandlers)
  {
    for (const auto& item2 : item1.second)
    {
      std::cout << "Remaining admin request handler: ?what=" << item1.first
        << " for " << targetName(item2.first) << std::endl;
      count++;
    }
  }

  if (count > 0)
  {
    std::cout << "INTERNAL ERROR: There were " << count << " remaining handlers" << std::endl;
  }
}

namespace
{
  std::string plugin_name(const SmartMetPlugin* plugin)
  {
    return plugin ? plugin->getPluginName() : "<builtin>";
  }
}


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
  itsAdminHandlerInfo->itsAdminUri = theUri;
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

  const std::string pluginName = plugin_name(thePlugin);

  // Get IP filter for the content handler (if any)
  std::shared_ptr<IPFilter::IPFilter> filter;
  if (thePlugin)
  {
    auto itsFilterIterator = itsIPFilters.find(Fmi::ascii_tolower_copy(pluginName));
    if (itsFilterIterator != itsIPFilters.end())
      filter = itsFilterIterator->second;

    std::cout << Spine::log_time_str() << ANSI_BOLD_ON << ANSI_FG_GREEN << " Registered "
          << (isPrivate ? "private " : "") << "URI " << theUri << " for plugin "
          << pluginName << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << std::endl;
  }
  else
  {
    std::cout << Spine::log_time_str() << ANSI_BOLD_ON << ANSI_FG_GREEN << " Registered "
          << (isPrivate ? "private " : "") << " request handler for URI "
          << theUri << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << std::endl;
    // We have built in admin request handler (no plugin). Use its IP filter if available.
    if (theUri == *itsAdminHandlerInfo->itsAdminUri)
      filter = itsAdminHandlerInfo->itsIPFilter;
  }

  // Create a new handler and add it to the map
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

std::size_t ContentHandlerMap::removeContentHandlers(HandlerTarget currentTarget)
try
{
  WriteLock lock(itsContentMutex);

  std::size_t count = 0;
  // We do not support content handlers for engines
  if (!std::holds_alternative<SmartMetEngine*>(currentTarget))
  {
    // For some reason std::get_if<> returned 1 for NoTarget, so we need to check it separately (c++20, clang++-18)
    const bool no_target = std::holds_alternative<NoTarget>(currentTarget);
    const SmartMetPlugin* thePlugin = no_target ? nullptr : std::get<SmartMetPlugin*>(currentTarget);
    for (auto it = itsHandlers.begin(); it != itsHandlers.end();)
    {
      auto curr = it++;
      if (curr->second and curr->second->usesPlugin(thePlugin))
      {
        const std::string uri = curr->first;
        const std::string name = targetName(currentTarget);
        const bool is_private = curr->second->isPrivate();
        itsHandlers.erase(curr);
        itsUriPrefixes.erase(uri);
        count++;
        std::cout << Spine::log_time_str() << ANSI_BOLD_ON << ANSI_FG_GREEN << " Removed "
            << (no_target ? "builtin " : "") << (is_private ? "private " : "")
            << "URI " << uri << (no_target ? ""s : " provided by " + name)
            << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << std::endl;
      }
    }
  }

  // Remove all admin request handlers provided by the target.
  for (auto it1 = itsAdminRequestHandlers.begin(); it1 != itsAdminRequestHandlers.end(); )
  {
    int erased = 0;
    auto curr = it1++;
    const std::string what = curr->first;
    for (auto it2 = curr->second.begin(); it2 != curr->second.end(); )
    {
      auto item = it2++;
      const HandlerTarget target = item->first;
      if (target == currentTarget)
      {
        curr->second.erase(item);
        std::cout << Spine::log_time_str() << ANSI_BOLD_ON << ANSI_FG_BLUE << " Removed "
            << adminRequestName(currentTarget, what)
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


bool ContentHandlerMap::hasHandlerView(const std::string& resource_) const
{
  std::string resource = resource_;
  ReadLock lock(itsContentMutex);
  for (const auto& item : itsUriPrefixes)
  {
    std::size_t len = item.length();
    if (resource.substr(0, len) == item && (resource.length() == len || resource[len] == '/'))
    {
      resource = item;
      break;
    }
  }

  // Try to find a content handler
  return itsHandlers.find(resource) != itsHandlers.end();
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

  const auto kill_cleaner_thread = [&]()
  {
    if (itsLogCleanerThread.get() != nullptr && itsLogCleanerThread->joinable())
    {
      itsLogCleanerThread->interrupt();
      itsLogCleanerThread->join();
    }
  };

  itsLoggingEnabled = loggingEnabled;

  if (itsLoggingEnabled)
  {
    // See if cleaner thread is running for some reason
    kill_cleaner_thread();

    // Launch log cleaner thread
    itsLogCleanerThread.reset(new boost::thread(std::bind(&ContentHandlerMap::cleanLog, this)));

    // Set log cleanup time
    itsLogLastCleaned = Fmi::SecondClock::local_time();
  }
  else
  {
    // Status set to false, make the transition true->false
    // Erase log, stop cleaning thread
    kill_cleaner_thread();
  }

  // Set logging status for ALL plugins (itsContentMutex is already locked in executeAdminRequest, so no
  // additional locking is needed here. This is safe because no other operation can change itsHandlers while this
  // operation is done)
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
    try
    {
      boost::this_thread::sleep_for(boost::chrono::seconds(5));

      auto firstValidTime = Fmi::SecondClock::local_time() - maxAge;

      ReadLock lock(itsContentMutex);
      for (auto& handlerPair : itsHandlers)
      {
        if (Reactor::isShuttingDown())
          return;

        handlerPair.second->cleanLog(firstValidTime, true);
      }

      if (itsLogLastCleaned < firstValidTime)
      {
        itsLogLastCleaned = firstValidTime;
      }
    }
    catch(const std::exception& e)
    {
      std::cerr << e.what() << '\n';
    }
  }
}
catch (...)
{
  std::cout << Fmi::Exception::Trace(BCP, "Operation failed!") << std::endl;
}

bool ContentHandlerMap::isURIPrefix(const std::string& uri) const
{
  return itsUriPrefixes.count(uri) > 0;
}



void ContentHandlerMap::handleAdminRequest(
    const HTTP::Request& request,
    HTTP::Response& response)
try
{
  Reactor* theReactor = getReactor();
  if (theReactor)
  {
    theReactor->executeAdminRequest(
        request,
        response,
        itsAdminHandlerInfo->itsAdminAuthenticationCallback);
  }
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}


bool ContentHandlerMap::addAdminRequestHandlerImpl(
    HandlerTarget target,
    const std::string& what,
    AdminRequestAccess access,
    AdminRequestHandler theHandler,
    const std::string& description)
try
{
  bool unique = not std::holds_alternative<AdminBoolRequestHandler>(theHandler);

  WriteLock lock(itsContentMutex);

  std::shared_ptr<IPFilter::IPFilter> filter;

  auto handler = std::make_unique<AdminRequestInfo>();
  handler->what = what;
  handler->target = target;
  handler->requiresAuthentication = access == AdminRequestAccess::RequiresAuthentication;
  handler->isPublic = access == AdminRequestAccess::Public;
  handler->handler = theHandler;
  handler->description = description;

  if (handler->requiresAuthentication && !itsAdminHandlerInfo->itsAdminAuthenticationCallback)
  {
    std::cout << Spine::log_time_str() << ANSI_BOLD_ON << ANSI_FG_RED
              << " Admin request '" << what << "' registration ignored - "
              << "no authentication available"
              << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << std::endl;
    // No authentication callback available, ignore the request (do not report failure)
    return true;
  }

  // Try adding plugin entry for admin requests. It does not matter whether
  // plugin is already present as pos1.first is always valid is always valid
  const auto pos1 = itsAdminRequestHandlers.emplace(what,
    std::map<HandlerTarget, std::shared_ptr<AdminRequestInfo>>());

  // Should be new entry if unique==true
  if (pos1.second)
  {
    if (itsUniqueAdminRequests.count(what))
    {
      // Already defined and some earlier request required to be unique : report error
      const std::string name = targetName(target);
      std::ostringstream err;
      err << "Failed to add " << adminRequestName(target, what) << " (already defined and required to be unique)";
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
      err << "Failed to add " << adminRequestName(target, what) << " (already defined and required to be unique)";
      throw Fmi::Exception(BCP, err.str());
    }
  }

  auto& dest_map = pos1.first->second;

  const auto result = dest_map.emplace(target, std::move(handler));
  if (not result.second)
  {
    const std::string name = targetName(target);
    std::ostringstream err;
    err << "Failed to add " << adminRequestName(target, what) << " (already defined)";
    throw Fmi::Exception(BCP, err.str());
  }

  if (unique)
  {
    itsUniqueAdminRequests.insert(what);
  }

  std::cout << Spine::log_time_str() << ANSI_BOLD_ON << ANSI_FG_BLUE
            << " Registered " << adminRequestName(target, what)
            << ANSI_BOLD_OFF << ANSI_FG_DEFAULT
            << std::endl;

  return true;
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}

bool ContentHandlerMap::removeAdminRequestHandler(HandlerTarget target,
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
    std::function<bool(const HTTP::Request&, HTTP::Response&)> authCallback)
{
  try
  {
    // Force stack trace to be generated in case of an exception
    Fmi::Exception::ForceStackTrace force_stack_trace;

    const auto unknown_request =
      [&theResponse](bool isInfo, const std::string& what)
      {
        theResponse.setStatus(HTTP::Status::not_found);
        theResponse.setContent("Unknown "s
          + (isInfo ? "info" : "admin")
          + " request: "
          + what);
      };

    ReadLock lock(itsContentMutex);

    const std::string resource = theRequest.getResource();
    const bool isInfo = itsAdminHandlerInfo->itsInfoUri && resource == *itsAdminHandlerInfo->itsInfoUri;

    const auto what = theRequest.getParameter("what");
    if (not what)
    {
      theResponse.setStatus(HTTP::Status::bad_request);
      theResponse.setContent("Missing 'what' parameter\n");
      return false;
    }

    // Look for the handler
    const auto it = itsAdminRequestHandlers.find(*what);
    if (it == itsAdminRequestHandlers.end())
    {
      unknown_request(isInfo, *what);
      return false;
    }

    // Only accept public handlers for info requests
    if (isInfo)
    {
      bool isPublic = false;
      for (const auto& item : it->second)
      {
        if (item.second->isPublic)
        {
          isPublic = true;
          break;
        }
      }

      if (not isPublic)
      {
        unknown_request(isInfo, *what);
        return false;
      }
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
        try
        {
          theResponse.setContent (""); // Clear any previous content
          bool authPassed = authCallback(theRequest, theResponse);
          if (not authPassed)
          {
            if (theResponse.getContent() == "")
            {
              theResponse.setStatus(HTTP::Status::unauthorized);
              theResponse.setContent("Unauthorized");
            }
            return false;
          }
        }
        catch (const Fmi::Exception&)
        {
          theResponse.setStatus(HTTP::Status::bad_request);
          theResponse.setContent("Corrupt Authorization header");
          return false;
        }
      } else {
        theResponse.setStatus(HTTP::Status::forbidden);
        theResponse.setContent("Authentication required but callback not provided");
        return false;
      }
    }

    Reactor* reactor = getReactor();
    if (!reactor)
    {
      throw Fmi::Exception(BCP, "INTERNAL ERROR: Reactor not available");
    }

    // Make a local copy before unlocking the mutex
    bool ok = true;
    bool haveBoolAdminRequests = false;
    bool haveNonBoolAdminRequests = false;
    const std::map<HandlerTarget, std::shared_ptr<AdminRequestInfo>> handlers = it->second;
    for (const auto& item : handlers)
    {
      // FIXME: what to do if one handler of several throws an exception?
      const AdminRequestHandler& handler = item.second->handler;
      const bool is_bool_handler = std::holds_alternative<AdminBoolRequestHandler>(handler);
      haveBoolAdminRequests = haveBoolAdminRequests || is_bool_handler;
      haveNonBoolAdminRequests = haveNonBoolAdminRequests || !is_bool_handler;
    }

    if (haveBoolAdminRequests && haveNonBoolAdminRequests)
    {
      // Should never happen due to earlier checks when adding them
      throw Fmi::Exception(BCP, "INTERNAL ERROR: Mixed admin request handlers");
    }

    if (haveNonBoolAdminRequests && handlers.size() > 1)
    {
      // Should never happen due to earlier checks when adding them
      throw Fmi::Exception(BCP, "INTERNAL ERROR: Multiple non-bool admin request handlers");
    }

    //============================  FIXME  ====================
    // It is not thread safe to unlock theContentMutex here, but otherwise attempt to add
    // or remove content or admin request handlers will cause deadlock.
    //==========================================================
    lock.unlock();

    std::ostringstream errors;

    for (const auto& item : handlers)
    {
      // FIXME: what to do if one handler of several throws an exception?
      const AdminRequestHandler& handler = item.second->handler;
      if (isInfo && !item.second->isPublic)
      {
        // Skip non-public handlers when /info URI is used
        continue;
      }

      if (std::holds_alternative<AdminBoolRequestHandler>(handler))
      {
        const std::string id = "Handler: {" + targetName(item.second->target) + ":" + item.second->what
               + "} - " + item.second->description;
        try
        {
          bool currOk = handleAdminBoolRequest(
            errors,
            std::get<AdminBoolRequestHandler>(handler),
            *reactor,
            theRequest);
          ok = ok && currOk;
          errors << (currOk ? "OK    : " : "ERROR  ") << id << std::endl;
        }
        catch (...)
        {
          ok = false;
          errors << "ERROR : " << id << std::endl;
        }
      }
      else
      {
        if (std::holds_alternative<AdminStringRequestHandler>(handler))
        {
          handleAdminStringRequest(
            errors,
            std::get<AdminStringRequestHandler>(handler),
            *reactor,
            theRequest,
            theResponse);
        }
        else if (std::holds_alternative<AdminTableRequestHandler>(handler))
        {
          handleAdminTableRequest(
            errors,
            std::get<AdminTableRequestHandler>(handler),
            *reactor,
            theRequest, theResponse);
        }
        else if (std::holds_alternative<AdminCustomRequestHandler>(handler))
        {
          handleAdminCustomRequest(
            errors,
            std::get<AdminCustomRequestHandler>(handler),
            *reactor,
            theRequest,
            theResponse);
        }
        else
        {
          throw Fmi::Exception(BCP, "INTERNAL ERROR: Unknown admin request handler type");
        }
      }
    }

    // We return JSON, hence we should enable CORS
    theResponse.setHeader("Access-Control-Allow-Origin", "*");

      // Adding response headers

    const int expires_seconds = 60;
    Fmi::DateTime t_now = Fmi::SecondClock::universal_time();
    Fmi::DateTime t_expires = t_now + Fmi::Seconds(expires_seconds);
    std::shared_ptr<Fmi::TimeFormatter> tformat(Fmi::TimeFormatter::create("http"));
    std::string cachecontrol = "public, max-age=" + std::to_string(expires_seconds);
    std::string expiration = tformat->format(t_expires);
    std::string modification = tformat->format(t_now);

    theResponse.setHeader("Cache-Control", cachecontrol);
    theResponse.setHeader("Expires", expiration);
    theResponse.setHeader("Last-Modified", modification);

    // We allow JSON requests, hence we should enable CORS
    theResponse.setHeader("Access-Control-Allow-Origin", "*");

    /* This will need some thought
             if(response.first.size() == 0)
             {
             std::cerr << "Warning: Empty input for request "
             << theRequest.getOriginalQueryString()
             << " from "
             << theRequest.getClientIP()
             << std::endl;
             }
    */
#ifdef MYDEBUG
      std::cout << "Output:" << std::endl << response << std::endl;
#endif

    if (haveBoolAdminRequests)
    {
      theResponse.setStatus(ok ? HTTP::Status::ok : HTTP::Status::internal_server_error);
      theResponse.setContent(errors.str());
    }

    return true;
  }
  catch (...)
  {
    const std::string format = Spine::optional_string(theRequest.getParameter("format"), "debug");
    const bool isdebug = (format == "debug");

    // Catching all exceptions

    Fmi::Exception exception(BCP, "Request processing exception!", nullptr);
    exception.addParameter("URI", theRequest.getURI());
    exception.addParameter("ClientIP", theRequest.getClientIP());
    exception.addParameter("HostName", Spine::HostInfo::getHostName(theRequest.getClientIP()));
    exception.printError();

    if (isdebug)
    {
      // Delivering the exception information as HTTP content
      std::string fullMessage = exception.getHtmlStackTrace();
      theResponse.setContent(fullMessage);
      theResponse.setStatus(Spine::HTTP::Status::ok);
    }
    else
    {
      theResponse.setStatus(Spine::HTTP::Status::bad_request);
    }

    // Adding the first exception information into the response header

    std::string firstMessage = exception.what();
    boost::algorithm::replace_all(firstMessage, "\n", " ");
    if (firstMessage.size() > 300)
    firstMessage.resize(300);
    theResponse.setHeader("X-Admin-Error", firstMessage);
    return false;
  }
}

std::optional<std::string> ContentHandlerMap::getAdminUri() const
{
  return itsAdminHandlerInfo->itsAdminUri;
}

std::optional<std::string> ContentHandlerMap::getInfoUri() const
{
  return itsAdminHandlerInfo->itsInfoUri;
}

std::unique_ptr<SmartMet::Spine::Table> ContentHandlerMap::getAdminRequests() const
{
  return getAdminRequestsImpl(std::nullopt, false);
}

std::unique_ptr<SmartMet::Spine::Table> ContentHandlerMap::getTargetAdminRequests(HandlerTarget target) const
{
  return getAdminRequestsImpl(target, false);
}

std::unique_ptr<SmartMet::Spine::Table> ContentHandlerMap::getAdminRequestsImpl(
      std::optional<HandlerTarget> target,
      bool publicOnly) const
{
  int y = 0;
  auto result = std::make_unique<SmartMet::Spine::Table>();

  if (publicOnly)
  {
    result->setTitle("Info requests summary");
    result->setNames({"What", "Target", "Description"});
  }
  else
  {
    result->setTitle("Admin requests summary");
    result->setNames({"What", "Target", "Authentication", "Public", "Unique", "Description"});
  }

  ReadLock lock(itsContentMutex);
  for (const auto& item1 : itsAdminRequestHandlers)
  {
    const std::string& what = item1.first;
    for (const auto& item2 : item1.second)
    {
      if (publicOnly && !item2.second->isPublic)
        continue;

      if (target && item2.first != *target)
        continue;

      const std::string& plugin_name = targetName(item2.second->target);
      const std::string& description = item2.second->description;
      const std::string authInfo = item2.second->requiresAuthentication ? "yes" : "no";
      const std::string isPublic = item2.second->isPublic ? "yes" : "no";
      const std::string unique = itsUniqueAdminRequests.count(what) ? "yes" : "no";
      int col = 0;
      result->set(col++, y, what);
      result->set(col++, y, plugin_name);
      if (!publicOnly)
      {
        result->set(col++, y, authInfo);
        result->set(col++, y, isPublic);
        result->set(col++, y, unique);
      }
      result->set(col++, y, description);
      y++;
    }
  }
  return result;
}


bool ContentHandlerMap::addAdminBoolRequestHandler(
    HandlerTarget target,
    const std::string& what,
    AdminRequestAccess access,
    std::function<bool(Reactor&, const HTTP::Request&)> theHandler,
    const std::string& description)
{
  // FIXME: separate parameter for specifying whether request is public
  return addAdminRequestHandlerImpl(target, what, access,
    AdminRequestHandler(theHandler), description);
}


bool ContentHandlerMap::handleAdminBoolRequest(
    std::ostream& errors,
    const AdminBoolRequestHandler& handler,
    Reactor& reactor,
    const HTTP::Request& request)
try
{
  std::ostringstream info;
  return handler(reactor, request);
}
catch (const std::exception& err)
{
  // FIXME: use Fmi::Exception
  errors << "Exception: " << err.what() << std::endl;
  return false;
}
catch (...)
{
  errors << "Unknown exception" << std::endl;
  return false;
}

bool ContentHandlerMap::addAdminTableRequestHandler(
        HandlerTarget target,
        const std::string& what,
        AdminRequestAccess access,
        std::function<std::unique_ptr<Table>(Reactor&, const HTTP::Request&)> theHandler,
        const std::string& description)
{
  // FIXME: separate parameter for specifying whether request is public
  return addAdminRequestHandlerImpl(target, what, access,
      AdminRequestHandler(theHandler), description);
}


void ContentHandlerMap::handleAdminTableRequest(
    std::ostream& errors,
    const AdminTableRequestHandler& handler,
    Reactor& reactor,
    const HTTP::Request& request,
    HTTP::Response& response)
try
{
  using namespace SmartMet::Spine;
  std::unique_ptr<Table> result = handler(reactor, request);
  const std::string fmt = optional_string(request.getParameter("format"), result->getDefaultFormat());
  TableFormatterOptions opt;
  std::unique_ptr<TableFormatter> formatter(TableFormatterFactory::create(fmt));
  const std::string formattedResult = formatter->format(*result, {}, request, opt);
  std::string mime = formatter->mimetype() + "; charset=UTF-8";
  response.setHeader("Content-Type", mime);
  response.setContent(formattedResult);
  response.setStatus(HTTP::Status::ok);
}
catch (...)
{
  std::ostringstream msg;
  msg << Fmi::Exception(BCP, "Operation failed");
  response.setStatus(HTTP::Status::internal_server_error);
  response.setContent(msg.str());
}


bool ContentHandlerMap::addAdminStringRequestHandler(
        HandlerTarget target,
        const std::string& what,
        AdminRequestAccess access,
        std::function<std::string(Reactor&, const HTTP::Request&)> theHandler,
        const std::string& description)
{
  // FIXME: separate parameter for specifying whether request is public
  return addAdminRequestHandlerImpl(target, what, access,
    AdminRequestHandler(theHandler), description);
}


void ContentHandlerMap::handleAdminStringRequest(
    std::ostream& errors,
    const AdminStringRequestHandler& handler,
    Reactor& reactor,
    const HTTP::Request& request,
    HTTP::Response& response)
try
{
  std::string result = handler(reactor, request);
  response.setContent(result);
  response.setStatus(HTTP::Status::ok);
}
catch (...)
{
  std::ostringstream msg;
  msg << Fmi::Exception(BCP, "Operation failed");
  response.setStatus(HTTP::Status::internal_server_error);
  response.setContent(msg.str());
}


bool ContentHandlerMap::addAdminCustomRequestHandler(
        HandlerTarget target,
        const std::string& what,
        AdminRequestAccess access,
        std::function<void(Reactor&, const HTTP::Request&, HTTP::Response&)> theHandler,
        const std::string& description)
{
  // FIXME: separate parameter for specifying whether request is public
  return addAdminRequestHandlerImpl(target, what, access,
    AdminRequestHandler(theHandler), description);
}


void ContentHandlerMap::handleAdminCustomRequest(
    std::ostream& errors,
    const AdminCustomRequestHandler& handler,
    Reactor& reactor,
    const HTTP::Request& request,
    HTTP::Response& response)
try
{
  handler(reactor, request, response);
}
catch (...)
{
  std::ostringstream msg;
  msg << Fmi::Exception(BCP, "Operation failed");
  response.setStatus(HTTP::Status::internal_server_error);
  response.setContent(msg.str());
}


std::unique_ptr<Table> ContentHandlerMap::getLoggingRequest(
    Reactor& reactor,
    const HTTP::Request& theRequest) const
try
{
  std::unique_ptr<Table> result(new SmartMet::Spine::Table);
  result->setNames({"LoggingStatus"});

  bool isCurrentlyLogging = getLogging();
  if (isCurrentlyLogging)
    result->set(0, 0, "Enabled");
  else
    result->set(0, 0, "Disabled");

  return result;
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}

bool ContentHandlerMap::setLoggingRequest(
    Reactor& reactor,
    const HTTP::Request& theRequest)
{
    // First parse if logging status change is requested
    auto loggingFlag = theRequest.getParameter("status");
    if (!loggingFlag)
      throw Fmi::Exception(BCP, "Logging parameter value not set.");

    std::string flag = *loggingFlag;
    // Logging status change requested
    if (flag == "enable")
    {
      setLogging(true);
      return true;
    }
    if (flag == "disable")
    {
      setLogging(false);
      return true;
    }
    throw Fmi::Exception(BCP, "Invalid logging parameter value: " + flag);
}

std::unique_ptr<Table> ContentHandlerMap::serviceInfoRequest(
    Reactor&,
    const HTTP::Request&)
try
{
  std::unique_ptr<Table> result(new SmartMet::Spine::Table);
  result->setTitle("Available services");
  result->setNames({"   URI   ", "   Is prefix   ", "   Provided by   "});
  ReadLock lock(itsContentMutex);
  int row = 0;
  for (const auto& item : itsHandlers)
  {
    const std::unique_ptr<HandlerView>& handler = item.second;
    const std::string& name = item.first;
    const SmartMetPlugin* plugin = handler->getPlugin();
    result->set(0, row, name);
    result->set(1, row, (itsUriPrefixes.count(name) ? "yes" : "no"));
    result->set(2, row, (plugin ? plugin->getPluginName() + " plugin" : "<builtin>" ));
    row++;
  }
  return result;
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}

Reactor* ContentHandlerMap::getReactor()
{
  // Currently Reactor is class derived from ContentHandlerMap
  return dynamic_cast<Reactor*>(this);
}

ContentHandlerMap::AdminHandlerInfo::AdminHandlerInfo(const Options& options)
try
  : itsInfoUri("/info")  // FIXME: get from configuration instead
{
  maybe_setup_admin_handler(options);
  maybe_setup_info_handler(options);
}
catch (...)
{
  throw Fmi::Exception::Trace(BCP, "Operation failed!");
}

void ContentHandlerMap::AdminHandlerInfo::maybe_setup_admin_handler(const Options& options)
try
{
    std::string uri;
    options.itsConfig.lookupValue("admin.uri", uri);

    // Cannot handle admin requests here is no URI is provided
    // One can do this however by registering content handler in some plugin
    if (uri == "")
      return;

    itsAdminUri = uri;

    if (options.itsConfig.exists("admin.user") ^ options.itsConfig.exists("admin.password"))
    {
      throw Fmi::Exception(BCP, "Config error in " + options.configfile
        + ": Both admin.user and admin.password or none of them must be provided together with admin.uri");
      return;
    }

    if (options.itsConfig.exists("admin.user") && options.itsConfig.exists("admin.password"))
    {
      std::string user, password;
      options.itsConfig.lookupValue("admin.user", user);
      options.itsConfig.lookupValue("admin.password", password);

      itsAuthenticator.reset(new HTTP::Authentication());
      itsAuthenticator->addUser(user, password);

      itsAdminAuthenticationCallback =
        std::bind(
          &HTTP::Authentication::authenticateRequest,
          itsAuthenticator.get(),
          p::_1,
          p::_2);
    }
    else
    {
      std::cout << Spine::log_time_str() << ANSI_BOLD_ON << ANSI_FG_RED
                << " WARNING: No authentication for admin requests provided." << '\n'
                << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << std::endl;
    }

    // Find the ip filters
    std::vector<std::string> filterTokens;
    lookupHostStringSettings(options.itsConfig, filterTokens, "admin.ip_filters");
    if (!filterTokens.empty())
    {
      itsIPFilter.reset(new IPFilter::IPFilter(filterTokens));
    }
}
catch (...)
{
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
}

void ContentHandlerMap::AdminHandlerInfo::maybe_setup_info_handler(const Options& options)
try
{
  (void) options;
  // FIXME: get the URI from the configuration and possibly other settings from configiration
}
catch (...)
{
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
}
