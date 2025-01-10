#include "Engine.h"
#include "HTTP.h"
#include "Value.h"
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
        this, "testFail", Reactor::AdminRequestAccess::Public,
        [](Reactor&, const HTTP::Request&) -> bool { return false; },
        "Failing bool engine admin request");

  reactor->addAdminBoolRequestHandler(
        this, "testOK", Reactor::AdminRequestAccess::RequiresAuthentication,
        [](Reactor&, const HTTP::Request&) -> bool { return true; },
        "Test engine admin handler with authentication");

  reactor->addAdminTableRequestHandler(
        this, "testTable", Reactor::AdminRequestAccess::Public,
        [](Reactor&, const HTTP::Request&) -> std::unique_ptr<Table>
        {
          std::unique_ptr<Table> result(new Table);
          result->setNames({"Value1", "Value2"});
          result->set(0, 0, "12");
          result->set(0, 1, "14");
          result->set(1, 0, "21");
          result->set(1, 1, "26");
          return result;
        },
        "Test engine admin table handler");
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
