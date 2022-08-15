#include "PluginTest.h"
#include <macgyver/DebugTools.h>
#include <boost/program_options.hpp>
#include <iostream>
#include <sstream>

using namespace std;

void prelude(SmartMet::Spine::Reactor& reactor, const std::string& handler_path)
{
  try
  {
    // Wait for qengine to finish
    int timeout = 300;
    auto handlers = reactor.getURIMap();
    while ((handlers.find(handler_path) == handlers.end()) && (--timeout >= 0))
    {
      sleep(1);
      handlers = reactor.getURIMap();
    }

    if (timeout < 0)
    {
      throw Fmi::Exception::Trace(BCP, "Timed out waiting for plugin to start");
    }

    cout << endl
         << "Testing " << handlers[handler_path] << "  plugin" << std::endl
         << "============================" << std::endl;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Prelude failed!", nullptr);
  }
}

void alarm_handler(int)
{
  std::cerr << std::endl << "Timeout: test terminated" << std::endl << std::endl;
  abort();
}

#define nm_help "help"
#define nm_config "reactor-config"
#define nm_input "input-dir"
#define nm_expected "expected-output-dir"
#define nm_failures "failures-dir"
#define nm_threads "num-threads"
#define nm_ignore "ignore"
#define nm_timeout "timeout"
#define nm_handler "handler"

int main(int argc, char* argv[])
{
  Fmi::Exception::force_stack_trace = true;

  std::setlocale(LC_ALL, "en_US.UTF-8");  // for iconv to work

  (void)printRequest;

  try
  {
    namespace po = boost::program_options;

    std::vector<std::string> ignore_lists;

    po::options_description desc("Allowed options");

    // clang-format off
    desc.add_options()
        (nm_help ",h", "Show this help message")
        (nm_handler ",H", po::value<std::string>(), "Handler path (for example '/wfs'")
        (nm_config ",c", po::value<std::string>(), "Reactor config file")
        (nm_input ",i", po::value<std::string>(), "Tests input directory (default 'input')")
        (nm_expected ",e", po::value<std::string>(), "Expected output directory (default 'output')")
        (nm_failures ",f", po::value<std::string>(), "Directory where to write actual output in case of failures (default value 'failures')")
        (nm_threads ",n", po::value<int>(), "Number of threads (default 10)")
        (nm_ignore ",I", po::value<std::vector<std::string> >(), "Optional parameter to specify files containing lists of tests to be skipped. File .testignore from input directory is used of none specified. May be provided 0 or more times")
        (nm_timeout, po::value<unsigned>(), "Timeout of entire test run in seconds (missing or value  0 means no timeout");
    // clang-format on

    po::variables_map opt;
    po::store(po::parse_command_line(argc, argv, desc), opt);

    po::notify(opt);

    if (opt.count(nm_help))
    {
      std::cout << desc << std::endl;
      exit(1);
    }

    SmartMet::Spine::Options options;
    if (opt.count(nm_config))
    {
      options.configfile = opt[nm_config].as<std::string>();
    }
    else
    {
      throw Fmi::Exception::Trace(BCP,
                                  "Mandatory command line option --reactor-config (or -c) missing");
    }
    options.quiet = true;
    options.defaultlogging = false;
    options.accesslogdir = "/tmp";

    if (Fmi::tracerPid())
    {
        signal(SIGALRM, SIG_IGN);
    } else {
        signal(SIGALRM, alarm_handler);
    }

    try
    {
      SmartMet::Spine::PluginTest tester;
      SmartMet::Spine::PreludeFunction prelude_funct;
      if (opt.count(nm_input))
      {
        tester.setInputDir(opt[nm_input].as<std::string>());
      }
      if (opt.count(nm_timeout))
      {
        alarm(opt[nm_timeout].as<unsigned>());
      }
      if (opt.count(nm_expected))
      {
        tester.setOutputDir(opt[nm_expected].as<std::string>());
      }
      if (opt.count(nm_failures))
      {
        tester.setFailDir(opt[nm_failures].as<std::string>());
      }
      if (opt.count(nm_threads))
      {
        tester.setNumberOfThreads(opt[nm_threads].as<int>());
      }
      if (opt.count(nm_ignore))
      {
        ignore_lists = opt[nm_ignore].as<std::vector<std::string> >();
      }
      if (opt.count(nm_handler))
      {
        const std::string handler_path = opt[nm_handler].as<std::string>();
        prelude_funct = [handler_path](SmartMet::Spine::Reactor& reactor)
        { prelude(reactor, handler_path); };
      }
      else
      {
        throw Fmi::Exception::Trace(BCP, "Mandatory command line option --handler (-H) missing");
      }

      for (const auto& fn : ignore_lists)
      {
        tester.addIgnoreList(fn);
      }

      return tester.run(options, prelude_funct);
    }
    catch (const libconfig::ParseException& err)
    {
      std::ostringstream msg;
      msg << "Exception '" << Fmi::current_exception_type()
          << "' thrown while parsing configuration file " << options.configfile << "' at line "
          << err.getLine() << ": " << err.getError();
      throw Fmi::Exception(BCP, msg.str());
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }

  return 1;
}
