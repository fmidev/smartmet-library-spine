#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <variant>
#include <boost/thread.hpp>
#include "LoggedRequest.h"
#include "IPFilter.h"
#include "HandlerView.h"
#include "Options.h"
#include "SmartMetEngine.h"
#include "SmartMetPlugin.h"
#include "Table.h"
#include <macgyver/DateTime.h>

namespace SmartMet
{
namespace Spine
{

class Reactor;

using URIMap = std::map<std::string, std::string>;

class ContentHandlerMap
{
public:
    /**
     * @brief Possible targets of admin requests
     *
     * Admin requests could be implemented by both plugins and engines.
     */
    using AdminRequestTarget = std::variant<SmartMetPlugin*, SmartMetEngine*>;

public:
    ContentHandlerMap(const Options& options);

    virtual ~ContentHandlerMap();

    /**
     * @brief Set handler fir cases when provided URI does not match any of available handlers
     *
     * Empty handler (default) means, that there is no special handler for no match cases.
     *
     * @param theHandler Handler function
     */
    void setNoMatchHandler(ContentHandler theHandler);

    /**
     * @brief Add a new public handler to the map
     *
     * @param thePlugin Pointer to the plugin that provides the handler
     * @param theUri URI to be handled
     * @param theHandler Handler function
     * @param handlesUriPrefix If true, the handler is used for all URIs starting with theUri
     * @retval true Handler added successfully
     *
     * Handlers added by this method are included in the response of URIMap
     */
    inline bool addContentHandler(SmartMetPlugin* thePlugin,
                                  const std::string& theUri,
                                  const ContentHandler& theHandler,
                                  bool handlesUriPrefix = false)
    {
        return addContentHandler(thePlugin, theUri, theHandler, handlesUriPrefix, false);
    }

    /**
     * @brief Add a new private handler to the map
     *
     * @param thePlugin Pointer to the plugin that provides the handler
     * @param theUri URI to be handled
     * @param theHandler Handler function
     * @param handlesUriPrefix If true, the handler is used for all URIs starting with theUri
     * @retval true Handler added successfully
     *
     * Handlers added by this method are not included in the response of URIMap and as result
     * not advertized to the frontend
     */
    inline bool addPrivateContentHandler(SmartMetPlugin* thePlugin,
                                         const std::string& theUri,
                                         const ContentHandler& theHandler,
                                         bool handlesUriPrefix = false)
    {
        return addContentHandler(thePlugin, theUri, theHandler, handlesUriPrefix, true);
    }

    /**
     * @brief Add a new handler to the map
     *
     * @param thePlugin Pointer to the plugin that provides the handler
     * @param theUri URI to be handled
     * @param theHandler Handler function
     * @param handlesUriPrefix If true, the handler is used for all URIs starting with theUri
     * @param isPrivate If true, the handler is not visible in the URIMap
     */
    bool addContentHandler(SmartMetPlugin* thePlugin,
                           const std::string& theUri,
                           const ContentHandler& theHandler,
                           bool handlesUriPrefix,
                           bool isPrivate);

    /**
     * @brief Remove all handlers provided by the plugin
     *
     * @param thePlugin Pointer to the plugin
     * @return Number of removed handlers
     */
    std::size_t removeContentHandlers(SmartMetPlugin* thePlugin);

    /**
     * @brief Get the handler for the given URI
     */
    HandlerView* getHandlerView(const HTTP::Request& theRequest);

    /**
     * @brief Add IP filters to the handler
     */
    void addIPFilters(const std::string& pluginName, const std::vector<std::string>& filterTokens);

    /**
     * @brief Get registred URIs (except private ones)
     */
    URIMap getURIMap() const;

    /*
    * @brief Get copy of the log
    */
    AccessLogStruct getLoggedRequests(const std::string& thePlugin) const;

    /**
     * @brief Get the plugin name for the given URI
     *
     * @param uri URI to be checked (including private handlers)
     * @return Plugin name or std::nullopt if not found
     */
    std::optional<std::string> getPluginName(const std::string& uri) const;

    /**
     * @brief Dump all handler URIs to specified stream
     */
    void dumpURIs(std::ostream& output) const;

    /**
     * @brief Set request logging activity on or off
     *
     * @param loggingEnable New logging status
     */
    void setLogging(bool loggingEnabled);

    /**
     *  @brief Set admin request URI
     *  @param uri URI to be used for admin requests
     */
    void setAdminUri(const std::string& uri);

    /**
     * @brief Get current logging status
     */
    inline bool getLogging() const
    {
        return itsLoggingEnabled;
    }

    /**
     * @brief Check whether provided value should be considered as URI prefix
     */
    bool isURIPrefix(const std::string& uri) const;

    /**
     * @brief Add a new admin request handler
     *
     * @param target Pointer to the plugin or engine that provides the handler
     * @param what Contents of 'what' field for this request
     * @param requiresAuthentication If true, the request requires authentication
     * @param theHandler Handler function
     * @param description Description of the request
     * @retval true Handler added successfully
     *
     * Admin requests are also removed when removeContentHandlers is called for the plugin
     */
    bool addAdminRequestHandler(AdminRequestTarget target,
                                const std::string& what,
                                bool requiresAuthentication,
                                const ContentHandler& theHandler,
                                const std::string& description,
                                bool unique = true);

    /**
     * @brief Remove admin request handler
     */
    bool removeAdminRequestHandler(AdminRequestTarget target,
                                   const std::string& what);

    /**
     *  @brief Execute admin request and return the result.
     *
     *  Performs authentication if required for request using authentication callback.
     */
    bool executeAdminRequest(
            const HTTP::Request& theRequest,
            HTTP::Response& theResponse,
            std::function<bool(const HTTP::Request&)> authCallback);

    std::unique_ptr<Table> getAdminRequests() const;

private:
    /**
     * @brief Clean log od old entries
     */
    void cleanLog();

    static std::string targetName(const AdminRequestTarget& target);

private:

    struct AdminRequestInfo
    {
        std::string what;
        AdminRequestTarget target;
        bool requiresAuthentication;
        ContentHandler handler;
        std::string description;

        std::string name() const;
    };

    const Options& itsOptions;

    std::optional<std::string> adminUri;

    /**
     * @brief Logging status (enabled/diasabled)
     *
     * Can be changed by setLogging method
     */
    std::atomic<bool> itsLoggingEnabled;

    /**
     * @brief Handler for cases when no match is found for the URI
     */
    std::unique_ptr<HandlerView> itsCatchNoMatchHandler;

    /**
     * @brief Handlers for URIs
     */
    std::map<std::string, std::unique_ptr<HandlerView>> itsHandlers;

    /**
     * @brief Admin request URI
     */
    std::optional<std::string> itsAdminUri;

    /**
     * @brief Admin request handlers
     */
    std::map<std::string, std::map<AdminRequestTarget, std::unique_ptr<AdminRequestInfo> > > itsAdminRequestHandlers;

    /**
     * @brief Admin requests which are unique (used to avoid duplicates)
     */
    std::set<std::string> itsUniqueAdminRequests;

    /**
     * @brief Admin requests which require authentication
     */
    std::set<std::string> itsAdminRequestsRequiringAuthentication;

    /**
     *   @brief URI prefixes that must be linked with handlers
     *   - index is begin part of URI (for example /edr/, which could handle esim.
     *     /edr/collection/something/area?... .
     */
    std::set<std::string> itsUriPrefixes;

    /**
     * @brief IP filters for the plugins
     */
    std::map<std::string, std::shared_ptr<IPFilter::IPFilter>> itsIPFilters;

    mutable MutexType itsContentMutex;
    mutable MutexType itsLoggingMutex;

    Fmi::DateTime itsLogLastCleaned;
    std::shared_ptr<boost::thread> itsLogCleanerThread;
};

}  // namespace Spine
}  // namespace SmartMet
