#include "Engine.h"
#include "HTTP.h"
#include <cmath>

using namespace SmartMet::Spine;
using namespace SmartMet::Engine::Test;

Engine::Engine(const char* theConfigFile)
{
}

SmartMet::Engine::Test::Engine::~Engine()
{
}

double SmartMet::Engine::Test::Engine::testFunct(double x)
{
    // Just something for testing
    return std::sin(x);
}

void SmartMet::Engine::Test::Engine::init()
{
  Reactor* reactor = Reactor::instance;

  reactor->addAdminBoolRequestHandler(
        this, "testFail", false,
        [](Reactor&, const HTTP::Request&) -> bool { return false; },
        "Failing bool engine admin request");

  reactor->addAdminBoolRequestHandler(
        this, "testOK", false,
        [](Reactor&, const HTTP::Request&) -> bool { return true; },
        "Test engine admin handler with authentication");
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
