#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include "IPFilter.h"
#include "HandlerView.h"
#include "Options.h"
#include "SmartMetPlugin.h"

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
                           bool handlesUriPrefix = false,
                           bool isPrivate = false);

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

private:
    const Options& itsOptions;
    std::atomic<bool> itsLoggingEnabled;
    std::unique_ptr<HandlerView> itsCatchNoMatchHandler;
    std::map<std::string, std::unique_ptr<HandlerView>> itsHandlers;
    std::set<std::string> itsUriPrefixes;
    std::map<std::string, std::shared_ptr<IPFilter::IPFilter>> itsIPFilters;

    mutable MutexType itsContentMutex;
    mutable MutexType itsLoggingMutex;
};

}  // namespace Spine
}  // namespace SmartMet
