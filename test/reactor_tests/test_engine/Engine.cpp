#include "Engine.h"
#include <cmath>

SmartMet::Engine::Test::Engine::Engine(const char* theConfigFile)
{
    (void)theConfigFile;
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
