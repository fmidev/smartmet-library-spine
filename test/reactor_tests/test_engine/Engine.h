#pragma once

#include <SmartMetEngine.h>
#include <Reactor.h>

namespace SmartMet
{
  namespace Engine
  {
    namespace Test
    {
      class Engine : public SmartMet::Spine::SmartMetEngine
      {
      public:
          Engine(const char* theConfigFile);

          ~Engine() override;

          double testFunct(double x);

      protected:
          void init() override;
          void shutdown() override;
      };
    }
  }
}
