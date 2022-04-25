#pragma once

#include <spine/Reactor.h>
#include <spine/SmartMetPlugin.h>
#include "../test_engine/Engine.h"

namespace SmartMet
{
  using namespace Spine;

  namespace Plugin
  {
    namespace Test
    {
      class Plugin : public SmartMetPlugin
      {
      public:
        Plugin(Spine::Reactor* theReactor, const char* theConfig);
        ~Plugin() override;
        const std::string& getPluginName() const override;
        int getRequiredAPIVersion() const override;
        bool queryIsFast(const Spine::HTTP::Request& theRequest) const override;

      protected:
        void init() override;
        void shutdown() override;
        void requestHandler(Spine::Reactor& theReactor,
                            const Spine::HTTP::Request& theRequest,
                            Spine::HTTP::Response& theResponse) override;

      private:
        std::string dump_params(const Spine::HTTP::Request& theRequest) const;

      private:
        ::SmartMet::Engine::Test::Engine* test_engine;
        Reactor* itsReactor = nullptr;
      };
    }
  }
}
