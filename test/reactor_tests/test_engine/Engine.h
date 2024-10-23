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

          static void testAdminHandler(Spine::Reactor& theReactor,
                                       const Spine::HTTP::Request& theRequest,
                                       Spine::HTTP::Response& theResponse);

      protected:
          void init() override;
          void shutdown() override;
      };
    }
  }
}
