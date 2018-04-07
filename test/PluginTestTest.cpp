#include "PluginTest.h"
#include <iostream>

using namespace std;

void prelude(SmartMet::Spine::Reactor& reactor)
{
  (void)reactor;
}
int not_actually_called()
{
  SmartMet::Spine::Options options;
  options.configfile = "cnf/plugin_test.conf";
  options.quiet = true;
  return SmartMet::Spine::PluginTest::test(options, prelude);
}

int main(void)
{
  // Actually only compile and link test
  std::cout << "\n"
            << "Testing PluginTest\n"
            << "==================\n"
            << "\n"
            << "tests: compile and link test passed\n";
  return 0;
}
