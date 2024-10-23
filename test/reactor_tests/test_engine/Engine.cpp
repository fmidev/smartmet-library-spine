#include "Engine.h"
#include "HTTP.h"
#include <cmath>

using namespace SmartMet::Spine;

SmartMet::Engine::Test::Engine::Engine(const char* theConfigFile)
{
  Reactor* reactor = Reactor::instance;
  reactor->addAdminRequestHandler(
        this, "engine-test", false, &Engine::testAdminHandler,
        "Test engine admin handler without authentication");

  reactor->addAdminRequestHandler(
        this, "test1", false, &Engine::testAdminHandler,
        "Test engine admin handler with authentication (duplicate with plugin)");
}

SmartMet::Engine::Test::Engine::~Engine()
{
  Reactor* reactor = Reactor::instance;
  reactor->removeAdminRequestHandler(this, "test1");
  reactor->removeAdminRequestHandler(this, "engine-test");
}

double SmartMet::Engine::Test::Engine::testFunct(double x)
{
    // Just something for testing
    return std::sin(x);
}

void SmartMet::Engine::Test::Engine::testAdminHandler(
      Spine::Reactor& theReactor,
      const Spine::HTTP::Request& theRequest,
      Spine::HTTP::Response& theResponse)
{
  std::vector<std::string> text;
  text.push_back("Hello from test engine");
  if (theResponse.getContent() != "")
    text.push_back(theResponse.getContent());
  std::sort(text.begin(), text.end());
  std::ostringstream output;
  for (const auto& item : text)
    output << item << '\n';
  theResponse.setContent(output.str());
  theResponse.setStatus(HTTP::Status::ok);
}

void SmartMet::Engine::Test::Engine::init()
{
}

void SmartMet::Engine::Test::Engine::shutdown()
{
}

// DYNAMIC MODULE CREATION TOOLS

extern "C" void* engine_class_creator(const char* configfile, void* /* user_data */)
{
  return new SmartMet::Engine::Test::Engine(configfile);
}

extern "C" const char* engine_name()
{
  return "Test";
}
