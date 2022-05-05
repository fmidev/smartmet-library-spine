#include "Plugin.h"
#include <json/json.h>

SmartMet::Plugin::Test::Plugin::Plugin(SmartMet::Spine::Reactor *theReactor, const char *theConfig)
    : test_engine(nullptr)
{
    if (theReactor->getRequiredAPIVersion() != SMARTMET_API_VERSION)
        throw Fmi::Exception(BCP, "Backend and Server API version mismatch");

    itsReactor = theReactor;
}

SmartMet::Plugin::Test::Plugin::~Plugin()
{
}

const std::string& SmartMet::Plugin::Test::Plugin::getPluginName() const
{
    static const std::string name = "Test plugin";
    return name;
}

int SmartMet::Plugin::Test::Plugin::getRequiredAPIVersion() const
{
    return SMARTMET_API_VERSION;
}

bool SmartMet::Plugin::Test::Plugin::queryIsFast(const HTTP::Request& theRequest) const
{
    return true;
}

void SmartMet::Plugin::Test::Plugin::init()
{
    test_engine = itsReactor->getEngine<SmartMet::Engine::Test::Engine>("Test");
    if (std::abs(test_engine->testFunct(1.2) - std::sin(1.2)) > 1.0e-12) {
        throw std::runtime_error("Engine test function call result is incorrect");
    }

    if (!itsReactor->addContentHandler(this,
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
            "/test_prefix/",
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
}

void SmartMet::Plugin::Test::Plugin::shutdown()
{
}

void SmartMet::Plugin::Test::Plugin::requestHandler(
    Reactor& theReactor,
    const HTTP::Request& theRequest,
    HTTP::Response& theResponse)
{
    (void) theReactor;
    theResponse.setContent(dump_params(theRequest));
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
