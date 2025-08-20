// ======================================================================
/*!
 * \brief Reactor options
 */
// ======================================================================

#include "Options.h"
#include "ConfigTools.h"
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <fstream>
#include <iostream>
#include <optional>
#include <filesystem>

namespace SmartMet
{
namespace
{
// Parse string "N" or "N%"
unsigned int parse_threads(const std::string& str)
{
  try
  {
    if (!boost::algorithm::ends_with(str, "%"))
      return Fmi::stoi(str);
    auto percent = Fmi::stoi(str.substr(0, str.size() - 1));

    if (percent == 0)
      return 0;  // fully disabled

    auto max_threads = std::thread::hardware_concurrency();
    auto num = max_threads * percent / 100;  // percentage of capacity
    return std::max(1U, num);                // at least one thread
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to parse thread setting").addParameter("Threads", str);
  }
}

// Parse config setting for number of threads
std::optional<unsigned int> parse_threads(const libconfig::Config& config,
                                            const std::string& name)
{
  try
  {
    if (!config.exists(name))
      return {};

    const auto& value = config.lookup(name);

    if (value.getType() == libconfig::Setting::TypeString)
    {
      std::string str = value;
      return parse_threads(str);
    }

    int num = value;
    return num;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to parse configuration variable")
        .addParameter("Parameter", name);
  }
}

}  // namespace

namespace Spine
{
// ----------------------------------------------------------------------
/*!
 * \brief Parse all options
 * \return True, if execution may continue as usual
 */
// ----------------------------------------------------------------------

bool Options::parse(int argc, char* argv[])
{
  try
  {
    if (!parseOptions(argc, argv))
      return false;

    parseConfig();

    // This second pass makes sure command line options
    // override configuration file options. Command line
    // options may detemine where the configuration file is,
    // hence multiple passes are needed since we cannot
    // change how the boost program_options parser works.

    parseOptions(argc, argv);
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse command line options
 * \return True, if execution may continue as usual
 */
// ----------------------------------------------------------------------

bool Options::parseOptions(int argc, char* argv[])
{
  try
  {
    namespace po = boost::program_options;

    po::options_description desc("Allowed options");

    const char* msghelp = "print out help message";
    const char* msgversion = "display version number";
    const char* msgdebug = "set debug mode on";
    const char* msgquiet = "set quiet mode on";
    const char* msgverbose = "set verbose mode on";
    const char* msgstacktrace = "enable stack trace on crashes";
    const char* msglogrequest = "print all incoming requests";
    const char* msgport = "set the HTTP port";
    const char* msgtimeout = "set the server timeout in seconds";
    const char* msglib = "set the library directory";
    const char* msgconf = "set the configuration file";
    const char* msguser = "set the username the server is running as";
    const char* msgzip = "enable compression";
    const char* msgziplimit = "compression size limit in bytes";
    const char* msgdefaultlog = "make request logs by default";
    const char* msglocale = "default locale";
    const char* msgnewhandler = "new_handler for OOM situations (default/bad_alloc/terminate)";

    const char* msgaccesslogdir = "access log directory";

    const char* msgadminthreads = "set the number of threads for admin queries (default=100%)";
    const char* msgadminrequeue = "set the maximum requeue size for admin queries";

    const char* msgslowthreads = "set the number of threads for slow queries (default=100%)";
    const char* msgslowrequeue = "set the maximum requeue size for slow queries";

    const char* msgfastthreads = "set the number of threads for fast queries (default=100%)";
    const char* msgfastrequeue = "set the maximum requeue size for fast queries";

    const char* msgminactiverequests =
        "set the initial number of allowed simultaneous active requests";
    const char* msgmaxactiverequests = "set the maximum number of simultaneous active requests";
    const char* msgmaxactiverestartrequests =
        "reset the maximum number of allowed simultaneous active requests when max has been "
        "reached";
    const char* msgactiverequestrate =
        "set the increase rate for maximum number of simultaneous active requests";

    const char* msgmaxrequestsize = "set the maximum allowed size for requests";

    std::string adminthreads_str;  // --> adminpool.minsize
    std::string slowthreads_str;   // --> slowpool.minsize
    std::string fastthreads_str;   // --> fastpool.minsize

    // clang-format off
    desc.add_options()("help,h", msghelp)("version,V", msgversion)(
        "adminthreads,N", po::value(&adminthreads_str), msgadminthreads)(
        "maxadminrequeuesize,Q", po::value(&adminpool.maxrequeuesize)->default_value(adminpool.maxrequeuesize), msgadminrequeue)(
        "slowthreads,N", po::value(&slowthreads_str), msgslowthreads)(
        "maxslowrequeuesize,Q", po::value(&slowpool.maxrequeuesize)->default_value(slowpool.maxrequeuesize), msgslowrequeue)(
        "fastthreads,n", po::value(&fastthreads_str), msgfastthreads)(
        "maxfastrequeuesize,q", po::value(&fastpool.maxrequeuesize)->default_value(fastpool.maxrequeuesize), msgfastrequeue)(
        "minactiverequests", po::value(&throttle.start_limit)->default_value(throttle.start_limit), msgminactiverequests)(
        "maxactiverequests", po::value(&throttle.limit)->default_value(throttle.limit), msgmaxactiverequests)(
        "maxactiverestartrequests", po::value(&throttle.restart_limit)->default_value(throttle.restart_limit), msgmaxactiverestartrequests)(
        "requestlimitrate", po::value(&throttle.increase_rate)->default_value(throttle.increase_rate), msgactiverequestrate)(
        "maxrequestsize", po::value(&maxrequestsize)->default_value(maxrequestsize), msgmaxrequestsize)(
        "debug,d", po::bool_switch(&debug)->default_value(debug), msgdebug)(
        "verbose,v", po::bool_switch(&verbose)->default_value(verbose), msgverbose)(
        "quiet", po::bool_switch(&quiet)->default_value(quiet), msgquiet)(
        "stacktrace", po::bool_switch(&quiet)->default_value(stacktrace), msgstacktrace)(
        "logrequests", po::bool_switch(&logrequests)->default_value(logrequests), msglogrequest)(
        "configfile,c", po::value(&configfile)->default_value(configfile), msgconf)(
        "libdir,L", po::value(&directory)->default_value(directory), msglib)(
        "locale,l", po::value(&locale)->default_value(locale), msglocale)(
        "port,p", po::value(&port)->default_value(port), msgport)(
        "timeout,t", po::value(&timeout)->default_value(timeout), msgtimeout)(
        "user,u", po::value(&username)->default_value(username), msguser)(
        "compress,z", po::bool_switch(&compress)->default_value(compress), msgzip)(
        "compresslimit,Z", po::value(&compresslimit)->default_value(compresslimit), msgziplimit)(
        "defaultlogging,e", po::bool_switch(&defaultlogging)->default_value(defaultlogging), msgdefaultlog)(
        "accesslogdir,a", po::value(&accesslogdir)->default_value(accesslogdir), msgaccesslogdir)(
        "new-handler", po::value(&new_handler)->default_value(new_handler), msgnewhandler);

    // clang-format on

    // We except no positional arguments
    po::positional_options_description p;

    po::variables_map opt;
    po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), opt);

    po::notify(opt);

    if (opt.count("version") != 0)
    {
      std::cout << "SmartMet Server (" << argv[0] << ") (compiled on " << __DATE__ << ' '
                << __TIME__ << ')' << std::endl;

      // Do not continue the startup
      return false;
    }

    if (opt.count("help") != 0)
    {
      std::cout << "Usage: " << argv[0] << " [options]" << std::endl
                << std::endl
                << desc << std::endl;

      return false;
    }

    if (throttle.start_limit > throttle.limit)
      throttle.start_limit = throttle.limit;

    if (!adminthreads_str.empty())
      adminpool.minsize = parse_threads(adminthreads_str);

    if (!slowthreads_str.empty())
      slowpool.minsize = parse_threads(slowthreads_str);

    if (!fastthreads_str.empty())
      fastpool.minsize = parse_threads(fastthreads_str);

    if (adminpool.minsize > 10000 || slowpool.minsize > 10000 || fastpool.minsize > 10000)
      throw Fmi::Exception(BCP, "The maximum number of threads is 10000!");

    if (compresslimit < 100)
      throw Fmi::Exception(BCP, "Compression size limit below 100 makes no sense!");

    if (throttle.start_limit == 0 || throttle.limit == 0 || throttle.increase_rate == 0)
      throw Fmi::Exception(BCP, "Active request settings must be > 0")
          .addParameter("Start limit", Fmi::to_string(throttle.start_limit))
          .addParameter("Limit", Fmi::to_string(throttle.limit))
          .addParameter("Increase rate", Fmi::to_string(throttle.increase_rate));

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the configuration file
 */
// ----------------------------------------------------------------------

void Options::parseConfig()
{
  try
  {
    if (configfile.empty())
      return;

    if (!std::filesystem::exists(configfile))
      throw Fmi::Exception(BCP, "Configuration file missing")
          .addParameter("configfile", configfile);

    if (verbose)
      std::cout << "Reading configuration file '" << configfile << "'" << std::endl;

    try
    {
      // Enable sensible relative include paths
      std::filesystem::path p = configfile;
      p.remove_filename();
      itsConfig.setIncludeDir(p.c_str());

      itsConfig.readFile(configfile.c_str());
      expandVariables(itsConfig);

      // Read options

      lookupHostSetting(itsConfig, port, "port");
      lookupHostSetting(itsConfig, encryptionEnabled, "encryption.enabled");
      lookupHostSetting(itsConfig, encryptionCertificateFile, "encryption.certificatefile");

      if (!encryptionCertificateFile.empty() && encryptionCertificateFile[0] != '/')
        encryptionCertificateFile = p.string() + "/" + encryptionCertificateFile;

      lookupHostSetting(itsConfig, encryptionPrivateKeyFile, "encryption.privatekeyfile");
      if (!encryptionPrivateKeyFile.empty() && encryptionPrivateKeyFile[0] != '/')
        encryptionPrivateKeyFile = p.string() + "/" + encryptionPrivateKeyFile;

      lookupHostSetting(itsConfig, encryptionPasswordFile, "encryption.passwordfile");
      if (!encryptionPasswordFile.empty() && encryptionPasswordFile[0] != '/')
        encryptionPasswordFile = p.string() + "/" + encryptionPasswordFile;

      lookupHostSetting(itsConfig, encryptionPassword, "encryption.password");
      lookupHostSetting(itsConfig, username, "user");
      lookupHostSetting(itsConfig, directory, "libdir");
      lookupHostSetting(itsConfig, timeout, "timeout");
      lookupHostSetting(itsConfig, locale, "locale");
      lookupHostSetting(itsConfig, verbose, "verbose");
      lookupHostSetting(itsConfig, debug, "debug");
      lookupHostSetting(itsConfig, quiet, "quiet");
      lookupHostSetting(itsConfig, stacktrace, "stacktrace");
      lookupHostSetting(itsConfig, logrequests, "logrequests");
      lookupHostSetting(itsConfig, compress, "compress");
      lookupHostSetting(itsConfig, compresslimit, "compresslimit");
      lookupHostSetting(itsConfig, defaultlogging, "defaultlogging");
      lookupHostSetting(itsConfig, lazylinking, "lazylinking");
      lookupHostSetting(itsConfig, accesslogdir, "accesslogdir");

      lookupHostSetting(itsConfig, throttle.limit, "maxactiverequests");     // old variable name
      lookupHostSetting(itsConfig, throttle.limit, "activerequests.limit");  // new variable name
      lookupHostSetting(itsConfig, throttle.start_limit, "activerequests.start_limit");
      lookupHostSetting(itsConfig, throttle.increase_rate, "activerequests.increase_rate");

      // Default restart_limit is start_limit when reading the configuration file.
      throttle.restart_limit = throttle.start_limit;
      lookupHostSetting(itsConfig, throttle.restart_limit, "activerequests.restart_limit");

      auto adminpool_minsize = parse_threads(itsConfig, "adminpool.maxthreads");
      if (adminpool_minsize)
        adminpool.minsize = *adminpool_minsize;

      auto slowpool_minsize = parse_threads(itsConfig, "slowpool.maxthreads");
      if (slowpool_minsize)
        slowpool.minsize = *slowpool_minsize;

      auto fastpool_minsize = parse_threads(itsConfig, "fastpool.maxthreads");
      if (fastpool_minsize)
        fastpool.minsize = *fastpool_minsize;

      lookupHostSetting(itsConfig, adminpool.maxrequeuesize, "adminpool.maxrequeuesize");
      lookupHostSetting(itsConfig, slowpool.maxrequeuesize, "slowpool.maxrequeuesize");
      lookupHostSetting(itsConfig, fastpool.maxrequeuesize, "fastpool.maxrequeuesize");

      lookupHostSetting(itsConfig, new_handler, "new_handler");
    }
    catch (libconfig::ParseException& e)
    {
      throw Fmi::Exception(BCP, "libconfig parser error")
          .addParameter("error", std::string("'") + e.getError() + "'")
          .addParameter("file", e.getFile())
          .addParameter("line", Fmi::to_string(e.getLine()));
    }
    catch (libconfig::SettingException& e)
    {
      throw Fmi::Exception(BCP, "libconfig setting error")
          .addParameter("error", std::string("'") + e.what() + "'")
          .addParameter("path", e.getPath());
    }
    catch (libconfig::ConfigException& e)
    {
      throw Fmi::Exception(BCP, "libconfig configuration error")
          .addParameter("error", std::string("'") + e.what() + "'");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to parse configuration file")
        .addParameter("configfile", configfile);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Print a summary of the options
 */
// ----------------------------------------------------------------------

void Options::report() const
{
  try
  {
    std::cout << ANSI_ITALIC_ON << "Starting with the following settings:\n"
              << "Debug mode\t\t\t= " << (debug ? "ON" : "OFF") << "\n"
              << "Quiet mode\t\t\t= " << (quiet ? "ON" : "OFF") << "\n"
              << "Log requests\t\t\t= " << (logrequests ? "ON" : "OFF") << "\n"
              << "Config file\t\t\t= " << configfile << "\n"
              << "Max fast threads\t\t= " << fastpool.minsize << "\n"
              << "Max fast queue size\t\t= " << fastpool.maxrequeuesize << "\n"
              << "Max slow threads\t\t= " << slowpool.minsize << "\n"
              << "Max slow queue size\t\t= " << slowpool.maxrequeuesize << "\n"
              << "Max admin threads\t\t= " << adminpool.minsize << "\n"
              << "Max admin queue size\t\t= " << adminpool.maxrequeuesize << "\n"
              << "Max active requests\t\t= " << throttle.limit << "\n"
              << "- at start\t\t\t= " << throttle.start_limit << "\n"
              << "- at slowdown\t\t\t= " << throttle.restart_limit << "\n"
              << "- increase rate\t\t\t= " << throttle.increase_rate << "\n"
              << "Port\t\t\t\t= " << port << "\n"
              << "Timeout\t\t\t\t= " << timeout << "\n"
              << "Access log directory\t\t= " << accesslogdir << "\n"
              << "Logs requests by default\t= " << defaultlogging << "\n"
              << "Lazy linking\t\t\t= " << (lazylinking ? "ON" : "OFF") << "\n"
              << "Stack trace on crash\t\t= " << (stacktrace ? "ON" : "OFF")
              << "\n"
                 "Out of memory handler\t\t= "
              << new_handler << "\n";

    std::cout << ANSI_ITALIC_OFF << std::endl;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
