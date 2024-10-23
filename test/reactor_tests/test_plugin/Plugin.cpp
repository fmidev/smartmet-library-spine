#include "Plugin.h"
#include "TableFormatterOptions.h"
#include "TableFormatterFactory.h"
#include <functional>
#include <sstream>
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
    static const std::string name = "Test";
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
            },
            true))
    {
        throw Fmi::Exception(BCP, "Failed to register test content handler (exact match)");
    }

    if (!itsReactor->addPrivateContentHandler(this,
            "/admin",
            [this](Reactor& theReactor,
                const HTTP::Request& theRequest,
                HTTP::Response& theResponse)
            {
                adminHandler(theReactor, theRequest, theResponse);
            },
            true))
    {
        throw Fmi::Exception(BCP, "Failed to register test content handler (exact match)");
    }

    if (!itsReactor->addAdminRequestHandler(
            this,
            "test1",
            false,
            &Plugin::testAdminHandler1,
            "Test admin handler without authentication",
            false))
    {
        throw Fmi::Exception(BCP, "Failed to register test content handler (exact match)");
    }

    if (!itsReactor->addAdminRequestHandler(
            this,
            "test2",
            true,
            &Plugin::testAdminHandler2,
            "Test admin handler with authentication"))
    {
        throw Fmi::Exception(BCP, "Failed to register test content handler (exact match)");
    }

    if (!itsReactor->addAdminRequestHandler(
            this,
            "list",
            false,
            &Plugin::testAdminHandler3,
            "List all admin requests"))
    {
        throw Fmi::Exception(BCP, "Failed to register test content handler (exact match)");
    }

}

void SmartMet::Plugin::Test::Plugin::shutdown()
{
    // Uncomment for checking reactor shutdown timeeot handling
    // Do NOT commit to GitHub uncommented
    //sleep(60);
}

void SmartMet::Plugin::Test::Plugin::requestHandler(
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

void SmartMet::Plugin::Test::Plugin::requestHandler2(
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

void SmartMet::Plugin::Test::Plugin::adminHandler(
                            Spine::Reactor& theReactor,
                            const Spine::HTTP::Request& theRequest,
                            Spine::HTTP::Response& theResponse)
{
    theReactor.executeAdminRequest(
        theRequest,
        theResponse,
        &Plugin::authCallback);
}

void SmartMet::Plugin::Test::Plugin::testAdminHandler1(
                            Spine::Reactor& theReactor,
                            const Spine::HTTP::Request& theRequest,
                            Spine::HTTP::Response& theResponse)
{
    std::vector<std::string> text;
    text.push_back("Hello from test plugin");
    if (theResponse.getContent() != "")
        text.push_back(theResponse.getContent());
    std::sort(text.begin(), text.end());
    std::ostringstream output;
    for (const auto& item : text)
        output << item << '\n';
    theResponse.setContent(output.str());
    theResponse.setStatus(HTTP::Status::ok);
}

void SmartMet::Plugin::Test::Plugin::testAdminHandler2(
                            Spine::Reactor& theReactor,
                            const Spine::HTTP::Request& theRequest,
                            Spine::HTTP::Response& theResponse)
{
    theResponse.setContent("Test admin handler 2\n");
    theResponse.setStatus(HTTP::Status::ok);
}

void SmartMet::Plugin::Test::Plugin::testAdminHandler3(
                            Spine::Reactor& theReactor,
                            const Spine::HTTP::Request& theRequest,
                            Spine::HTTP::Response& theResponse)
{
    const auto result = theReactor.getAdminRequests();
    std::unique_ptr<TableFormatter> formatter(TableFormatterFactory::create("json"));
    TableFormatterOptions opt;
    const std::string content = formatter->format(
        *result,
        {"what", "target", "auth", "unique", "description"},
        theRequest,
        opt);
    Json::Value doc;
    std::istringstream input(content);
    Json::CharReaderBuilder builder;
    Json::parseFromStream(builder, input, &doc, nullptr);
    std::ostringstream output;
    output << doc.toStyledString();
    theResponse.setContent(output.str());
    theResponse.setStatus(HTTP::Status::ok);
}


bool SmartMet::Plugin::Test::Plugin::authCallback(
                            const Spine::HTTP::Request& theRequest)
{
    const auto opt_user = theRequest.getParameter("user");
    const auto opt_password = theRequest.getParameter("password");
    return opt_user and *opt_user == "foo" and opt_password and *opt_password == "bar";
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
