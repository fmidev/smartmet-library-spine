#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <boost/thread.hpp>
#include "LoggedRequest.h"
#include "IPFilter.h"
#include "HandlerView.h"
#include "Options.h"
#include "SmartMetPlugin.h"
#include <macgyver/DateTime.h>

namespace SmartMet
{
namespace Spine
{

using URIMap = std::map<std::string, std::string>;

class ContentHandlerMap
{
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

private:
    /**
     * @brief Clean log od old entries
     */
    void cleanLog();

private:
    const Options& itsOptions;
    std::atomic<bool> itsLoggingEnabled;
    std::unique_ptr<HandlerView> itsCatchNoMatchHandler;
    std::map<std::string, std::unique_ptr<HandlerView>> itsHandlers;

    /**
     *   @brief URI prefixes that must be linked with handlers
     *   - index is begin part of URI (for example /edr/, which could handle esim.
     *     /edr/collection/something/area?... .
     */
    std::set<std::string> itsUriPrefixes;

    std::map<std::string, std::shared_ptr<IPFilter::IPFilter>> itsIPFilters;

    mutable MutexType itsContentMutex;
    mutable MutexType itsLoggingMutex;

    Fmi::DateTime itsLogLastCleaned;
    std::shared_ptr<boost::thread> itsLogCleanerThread;

};

}  // namespace Spine
}  // namespace SmartMet
