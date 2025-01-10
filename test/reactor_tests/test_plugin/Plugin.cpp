#include "Plugin.h"
#include "TableFormatterOptions.h"
#include "TableFormatterFactory.h"
#include <functional>
#include <sstream>
#include <json/json.h>

using namespace SmartMet::Plugin::Test;

Plugin::Plugin(SmartMet::Spine::Reactor *theReactor, const char *theConfig)
    : test_engine(nullptr)
{
    if (theReactor->getRequiredAPIVersion() != SMARTMET_API_VERSION)
        throw Fmi::Exception(BCP, "Backend and Server API version mismatch");

    itsReactor = theReactor;
}

Plugin::~Plugin()
{
}

const std::string& Plugin::getPluginName() const
{
    static const std::string name = "Test";
    return name;
}

int Plugin::getRequiredAPIVersion() const
{
    return SMARTMET_API_VERSION;
}

bool Plugin::queryIsFast(const HTTP::Request& theRequest) const
{
    return true;
}

void Plugin::init()
{
    test_engine = itsReactor->getEngine<SmartMet::Engine::Test::Engine>("Test");
    if (std::abs(test_engine->testFunct(1.2) - std::sin(1.2)) > 1.0e-12) {
        throw std::runtime_error("Engine test function call result is incorrect");
    }

    if (!itsReactor->addPrivateContentHandler(this,
            "/test",
            [this](Reactor& theReactor,
                const HTTP::Request& theRequest,
                HTTP::Response& theResponse)
            {
                requestHandler(theReactor, theRequest, theResponse);
            }))
    {
        throw Fmi::Exception(BCP, "Failed to register test content handler (exact match)");
    }

    if (!itsReactor->addContentHandler(this,
            "/test_prefix",
            [this](Reactor& theReactor,
                const HTTP::Request& theRequest,
                HTTP::Response& theResponse)
            {
                requestHandler(theReactor, theRequest, theResponse);
            },
            true))
    {
        throw Fmi::Exception(BCP, "Failed to register test content handler (exact match)");
    }

    if (!itsReactor->addPrivateContentHandler(this,
            "/uri-map",
            [this](Reactor& theReactor,
                const HTTP::Request& theRequest,
                HTTP::Response& theResponse)
            {
                requestHandler2(theReactor, theRequest, theResponse);
            }))
    {
        throw Fmi::Exception(BCP, "Failed to register test content handler (exact match)");
    }

    if (!itsReactor->addAdminBoolRequestHandler(
        this,
        "testFail",
        Reactor::AdminRequestAccess::Public,
        [](Reactor&, const HTTP::Request&) -> bool { return false; },
        "Failing bool admin handler"))
    {
        throw Fmi::Exception(BCP, "Failed to register test content handler");
    }

    if (!itsReactor->addAdminBoolRequestHandler(
        this,
        "testOK",
        Reactor::AdminRequestAccess::Public,
        [](Reactor&, const HTTP::Request&) -> bool { return true; }, "Failing OK admin handler"))
    {
        throw Fmi::Exception(BCP, "Failed to register test content handler");
    }

}

void Plugin::shutdown()
{
    // Uncomment for checking reactor shutdown timeeot handling
    // Do NOT commit to GitHub uncommented
    //sleep(60);
}

void Plugin::requestHandler(
    Reactor& theReactor,
    const HTTP::Request& theRequest,
    HTTP::Response& theResponse)
{
    (void) theReactor;
    bool supportsPost = theRequest.getResource() == "/test";
    if (checkRequest(theRequest, theResponse, supportsPost)) {
        return;
    }
    theResponse.setContent(dump_params(theRequest));
}

void Plugin::requestHandler2(
    Reactor& theReactor,
    const HTTP::Request& theRequest,
    HTTP::Response& theResponse)
{
    (void) theReactor;
    bool supportsPost = false;
    if (checkRequest(theRequest, theResponse, supportsPost)) {
        return;
    }
    std::ostringstream content;
    const auto uri_map = theReactor.getURIMap();
    for (const auto& item : uri_map) {
        content << item.first << " --> " << item.second << std::endl;
    }
    theResponse.setContent(content.str());
}

std::string SmartMet::Plugin::Test::Plugin::dump_params(const HTTP::Request& theRequest) const
{
    const auto params = theRequest.getParameterMap();
    Json::Value result;
    std::set<std::string> names;
    for (const auto& item : params) { names.insert(item.first); }
    for (const auto& name : names) {
        const auto& values = theRequest.getParameterList(name);
        if (values.empty()) {
            result["params"][name] = Json::nullValue;
        } else if (values.size() == 1) {
            result["params"][name] = *values.begin();
        } else {
            for (std::size_t i = 0; i < values.size(); i++) {
                result["params"][name][int(i)] = values[i];
            }
        }
    }
    result["content"] = theRequest.getContent();
    result["method"] = theRequest.getMethodString();
    result["URI"] = theRequest.getURI();
    return result.toStyledString();
}

extern "C" SmartMetPlugin *create(SmartMet::Spine::Reactor *them, const char *config)
{
  return new SmartMet::Plugin::Test::Plugin(them, config);
}

extern "C" void destroy(SmartMetPlugin *us)
{
  // This will call 'Plugin::~Plugin()' since the destructor is virtual
  delete us;
}
