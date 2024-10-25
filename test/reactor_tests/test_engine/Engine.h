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

          static bool testFailingAdminHandler(Spine::Reactor& theReactor,
                                              const Spine::HTTP::Request& theRequest);

          static bool testOKAdminHandler(Spine::Reactor& theReactor,
                                         const Spine::HTTP::Request& theRequest);

      protected:
          void init() override;
          void shutdown() override;
      };
    }
  }
}
