#include "Engine.h"
#include "HTTP.h"
#include <cmath>

using namespace SmartMet::Spine;
using namespace SmartMet::Engine::Test;

Engine::Engine(const char* theConfigFile)
{
  Reactor* reactor = Reactor::instance;

  reactor->addAdminRequestHandlers(
        this, {"testFail2", "testFail"}, false, &Engine::testFailingAdminHandler,
        "Test failing engine admin handler without authentication");

  reactor->addAdminRequestHandler(
        this, "testOK", false, &Engine::testOKAdminHandler,
        "Test engine admin handler with authentication");
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

bool Engine::testFailingAdminHandler(Reactor&, const HTTP::Request&)
{
    return false;
}

bool Engine::testOKAdminHandler(Reactor& theReactor, const HTTP::Request&)
{
    return true;
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
