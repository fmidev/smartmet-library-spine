#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
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

namespace HTTP
{
    class Authentication;
}

using URIMap = std::map<std::string, std::string>;

class ContentHandlerMap
{
public:
    /**
     * @brief Admin requests without target
     *
     * Note that one still needs top level handler to actually handle these.
     */
    using NoTarget = std::monostate;

    /**
     * @brief Possible targets of requests
     *
     * Content handler can be associated with a plugins only. The same type is
     * however being used to simplify implementation also for cleaning up
     * context handlers.
     *
     * Admin requests could be implemented by both plugins and engines
     * and also as stand-alone without any targets
     */
    using HandlerTarget = std::variant<NoTarget,
                                       SmartMetPlugin*,
                                       SmartMetEngine*>;

    using AdminBoolRequestHandler = std::function<bool(Reactor&, const HTTP::Request&)>;
    using AdminTableRequestHandler = std::function<std::unique_ptr<Table>(Reactor&, const HTTP::Request&)>;
    using AdminStringRequestHandler = std::function<std::string(Reactor&, const HTTP::Request&)>;
    using AdminCustomRequestHandler = std::function<void(Reactor&, const HTTP::Request&, HTTP::Response&)>;

    using AdminRequestHandler = std::variant<AdminBoolRequestHandler,
                                             AdminTableRequestHandler,
                                             AdminStringRequestHandler,
                                             AdminCustomRequestHandler>;

    /**
     *  @brief Handler for authentication
     *
     *  @param theRequest Request to be authenticated
     *  @param theResponse Response must be unchanged when authetication succeeds.
     *               Callback is expected to fill in response when authentication
     *               fails or some other error occurs. Method executeAdminRequest
     *               will however take care of setting the correct status code if
     *               that is not done by the callback.
     *  @retval true Authentication succeeded
     *  @retval false Authentication failed (or an error occurred)
     */
    using AuthenticationCallback = std::function<bool(const HTTP::Request&, HTTP::Response&)>;

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
    std::size_t removeContentHandlers(HandlerTarget thePlugin);

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
     * @brief Add admin bool request handler
     *
     * Returns true or false. If the handler returns false, the response status
     * is 500.
     *
     * There could be more than one handler of the same type for the same request
     * and all will be executed (order is not guaranteed to be the same as
     * registrated). Error code is 500 if at least one handler returns false.
     *
     * All formatting is done by the caller
     */
    bool addAdminBoolRequestHandler(
        HandlerTarget target,
        const std::string& what,
        bool requiresAuthentication,
        std::function<bool(Reactor&, const HTTP::Request&)> theHandler,
        const std::string& description);

    /**
     * @brief Add admin table request handler
     *
     * Returns table with data (one should set also column names and optinally
     * the title for the table). Formatting is done by the caller
     *
     * This request type is requires to be unique (only one handler per request name)
     */
    bool addAdminTableRequestHandler(
        HandlerTarget target,
        const std::string& what,
        bool requiresAuthentication,
        std::function<std::unique_ptr<Table>(Reactor&, const HTTP::Request&)> theHandler,
        const std::string& description);

    /**
     * @brief Add admin string request handler
     *
     * Returns string. Formatting is done by the caller
     *
     * This request type is requires to be unique (only one handler per request name)
     */
    bool addAdminStringRequestHandler(
        HandlerTarget target,
        const std::string& what,
        bool requiresAuthentication,
        std::function<std::string(Reactor&, const HTTP::Request&)> theHandler,
        const std::string& description);

    /**
     * @brief Add admin custom request handler
     *
     * User is fully responsible for fillin in the response
     *
     * This request type is requires to be unique (only one handler per request name)
     */
    bool addAdminCustomRequestHandler(
        HandlerTarget target,
        const std::string& what,
        bool requiresAuthentication,
        std::function<void(Reactor&, const HTTP::Request&, HTTP::Response&)> theHandler,
        const std::string& description);

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
     *
     * This method does not support providing std::bind results directly as theHandler
     * due to C++ limitations: 2 conversions required
     *    (std::bind result -> std::function -> std::variant)
     * Use wrapper function for each type to workaround this problem
     */
    bool addAdminRequestHandlerImpl(HandlerTarget target,
                                    const std::string& what,
                                    bool requiresAuthentication,
                                    AdminRequestHandler theHandler,
                                    const std::string& description);

    /**
     * @brief Remove admin request handler
     *
     * One should only use this method to remove handlers if that is required
     * before the plugin is removed. Otherwise removeContentHandlers handles also
     * removal of related admin request handlers
     */
    bool removeAdminRequestHandler(HandlerTarget target,
                                   const std::string& what);

    /**
     *  @brief Execute admin request and return the result.
     *
     *  Performs authentication if required for request using authentication callback.
     */
    bool executeAdminRequest(
            const HTTP::Request& theRequest,
            HTTP::Response& theResponse,
            std::function<bool(const HTTP::Request&, HTTP::Response&)> authCallback);

    /**
     * @brief Set admin request authentication callback
     *
     * Only needed if some admin requests require authentication and these
     * requests are handled through this class (itsAdminUri is not empty)
     */
    //void setAdminAuthenticationCallback(AuthenticationCallback callback);

    std::unique_ptr<Table> getAdminRequests() const;

private:
    /**
     * @brief Clean log od old entries
     */
    void cleanLog();

    void handleAdminRequest(
            const HTTP::Request& request,
            HTTP::Response& response);

    bool handleAdminBoolRequest(
            std::ostream& errors,
            const AdminBoolRequestHandler& handler,
            Reactor& reactor,
            const HTTP::Request& request);

    void handleAdminTableRequest(
            std::ostream& errors,
            const AdminTableRequestHandler& handler,
            Reactor& reactor,
            const HTTP::Request& request,
            HTTP::Response& response);

    void handleAdminStringRequest(
            std::ostream& errors,
            const AdminStringRequestHandler& handler,
            Reactor& reactor,
            const HTTP::Request& request,
            HTTP::Response& response);

    void handleAdminCustomRequest(
            std::ostream& errors,
            const AdminCustomRequestHandler& handler,
            Reactor& reactor,
            const HTTP::Request& request,
            HTTP::Response& response);

    std::unique_ptr<Table> getLoggingRequest(
            Reactor& reactor,
            const HTTP::Request& theRequest) const;

    bool setLoggingRequest(
            Reactor& reactor,
            const HTTP::Request& theRequest);

    std::unique_ptr<Table> serviceInfoRequest(
            Reactor&,
            const HTTP::Request&);

private:

    Reactor* getReactor();

    struct AdminRequestInfo
    {
        std::string what;
        HandlerTarget target;
        bool requiresAuthentication;
        AdminRequestHandler handler;
        std::string description;

        //std::string name() const;

        /**
         * @brief Check if the request is unique
         *
         * All requests except those which return bool must be unique
         * (cannot be registered twice from different plugins/engines)
         */
        bool unique() const;
    };

    /**
     * @brief Information about top level admin request handler
     *
     * This handler is registrated by addPrivateContentHandler method when
     * present (konfiguration settings) and is used to handle admin requests
     *
     * An alternative is to leave it empty and handle admin requests in the
     * some plugin (like smartmet-plugin-admin). In this case this structure is
     * not used
     *
     * These fields below are put into separate struct to avoid ABI breakage
     * if changes are made to AdminHandlerInfo
     */
    struct AdminHandlerInfo
    {
        /**
         * @brief Admin request URI. Default is empty
         */
        std::optional<std::string> itsAdminUri;

        /**
         * @brief Admin request authentication callback
         *
         * Admin requests thet require authentication is blocked if not set
         * (returns 403 Forbidden) or if the callback returns false
         */
        AuthenticationCallback itsAdminAuthenticationCallback;

        std::unique_ptr<HTTP::Authentication> itsAuthenticator;

        std::shared_ptr<IPFilter::IPFilter> itsIPFilter;

        AdminHandlerInfo(const Options& options);
    };

    const Options& itsOptions;

    /**
     * @brief Logging status (enabled/diasabled)
     *
     * Can be changed by setLogging method
     */
    std::atomic<bool> itsLoggingEnabled{false};

    /**
     * @brief Handler for cases when no match is found for the URI
     */
    std::unique_ptr<HandlerView> itsCatchNoMatchHandler;

    /**
     * @brief Handlers for URIs
     */
    std::map<std::string, std::unique_ptr<HandlerView>> itsHandlers;


    /**
     * @brief Admin request handlers
     */
    std::map
    <
        std::string,
        std::map
        <
            HandlerTarget,
            std::shared_ptr<AdminRequestInfo>
        >
    > itsAdminRequestHandlers;

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

    std::unique_ptr<AdminHandlerInfo> itsAdminHandlerInfo;

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
