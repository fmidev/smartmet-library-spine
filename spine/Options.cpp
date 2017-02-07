// ======================================================================
/*!
 * \brief Reactor options
 */
// ======================================================================

#include "Options.h"
#include "Exception.h"
#include <macgyver/AnsiEscapeCodes.h>
#include <boost/program_options.hpp>
#include <boost/filesystem/operations.hpp>
#include <iostream>

namespace SmartMet
{
namespace Spine
{
namespace
{
const char* default_configfile = "/etc/smartmet/smartmet.conf";
const char* default_libdir = "/usr/share/smartmet";
const char* default_locale = "fi_FI.UTF-8";
const char* default_accesslogdir = "/var/log/smartmet";
const unsigned int default_port = 8080;
const unsigned int default_maxthreads = 10;
const unsigned int default_timeout = 60;
const unsigned int default_maxrequeuesize = 100;
const unsigned int default_compresslimit = 1000;
}

// ----------------------------------------------------------------------
/*!
 * \brief The options hold two instances of pool options
 */
// ----------------------------------------------------------------------

PoolOptions::PoolOptions() : minsize(default_maxthreads), maxrequeuesize(default_maxrequeuesize)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief The constructor merely sets the defaults
 */
// ----------------------------------------------------------------------

Options::Options()
    : port(default_port),
      timeout(default_timeout),
      directory(default_libdir),
      configfile(default_configfile),
      locale(default_locale),
      verbose(false),
      quiet(false),
      debug(false),
      logrequests(false),
      compress(false),
      compresslimit(default_compresslimit),
      defaultlogging(true),
      lazylinking(true),
      accesslogdir(default_accesslogdir)
{
}

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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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

    const char* msgaccesslogdir = "access log directory";

    const char* msgslowthreads = "set the initial number of slow queries";
    const char* msgslowrequeue = "set the maximum requeue size for slow queries";

    const char* msgfastthreads = "set the initial number of fast queries";
    const char* msgfastrequeue = "set the maximum requeue size for fast queries";

    desc.add_options()("help,h", msghelp)("version,V", msgversion)(
        "slowthreads,N",
        po::value(&slowpool.minsize)->default_value(slowpool.minsize),
        msgslowthreads)("maxslowrequeuesize,Q",
                        po::value(&slowpool.maxrequeuesize)->default_value(fastpool.maxrequeuesize),
                        msgslowrequeue)(
        "fastthreads,n",
        po::value(&fastpool.minsize)->default_value(fastpool.minsize),
        msgfastthreads)("maxfastrequeuesize,q",
                        po::value(&fastpool.maxrequeuesize)->default_value(fastpool.maxrequeuesize),
                        msgfastrequeue)(
        "debug,d", po::bool_switch(&debug)->default_value(debug), msgdebug)(
        "verbose,v", po::bool_switch(&verbose)->default_value(verbose), msgverbose)(
        "quiet", po::bool_switch(&quiet)->default_value(quiet), msgquiet)(
        "logrequests", po::bool_switch(&logrequests)->default_value(logrequests), msglogrequest)(
        "configfile,c", po::value(&configfile)->default_value(configfile), msgconf)(
        "libdir,L", po::value(&directory)->default_value(directory), msglib)(
        "locale,l", po::value(&locale)->default_value(locale), msglocale)(
        "port,p", po::value(&port)->default_value(port), msgport)(
        "timeout,t", po::value(&timeout)->default_value(timeout), msgtimeout)(
        "user,u", po::value(&username)->default_value(username), msguser)(
        "compress,z", po::bool_switch(&compress)->default_value(compress), msgzip)(
        "compresslimit,Z", po::value(&compresslimit)->default_value(compresslimit), msgziplimit)(
        "defaultlogging,e",
        po::bool_switch(&defaultlogging)->default_value(defaultlogging),
        msgdefaultlog)(
        "accesslogdir,a", po::value(&accesslogdir)->default_value(accesslogdir), msgaccesslogdir);

    po::variables_map opt;
    po::store(po::command_line_parser(argc, argv).options(desc).run(), opt);

    po::notify(opt);

    if (opt.count("version") != 0)
    {
      std::cout << "SmartMet Server (" << argv[0] << ") (compiled on " << __DATE__ << ' '
                << __TIME__ << ')' << std::endl;

      // Do not continue the startup
      return false;
    }

    if (opt.count("help"))
    {
      std::cout << "Usage: " << argv[0] << " [options]" << std::endl
                << std::endl
                << desc << std::endl;

      return false;
    }

    if (slowpool.minsize > 10000 || fastpool.minsize > 10000)
    {
      throw SmartMet::Spine::Exception(BCP, "The maximum number of threads is 10000!");
    }

    if (compresslimit < 100)
    {
      throw SmartMet::Spine::Exception(BCP, "Compression size limit below 100 makes no sense!");
    }

    return true;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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

    if (verbose)
    {
      std::cout << "Reading configuration file '" << configfile << "'" << std::endl;
    }

    itsConfig.reset(new ConfigBase(configfile, "SmartMet"));

    // Read options

    port = itsConfig->get_optional_config_param("port", port);
    username = itsConfig->get_optional_config_param("user", username);
    directory = itsConfig->get_optional_config_param("libdir", directory);
    timeout = itsConfig->get_optional_config_param("timeout", timeout);
    locale = itsConfig->get_optional_config_param("locale", locale);
    verbose = itsConfig->get_optional_config_param("verbose", verbose);
    debug = itsConfig->get_optional_config_param("debug", debug);
    quiet = itsConfig->get_optional_config_param("quiet", quiet);
    logrequests = itsConfig->get_optional_config_param("logrequests", logrequests);
    compress = itsConfig->get_optional_config_param("compress", compress);
    compresslimit = itsConfig->get_optional_config_param("compresslimit", compresslimit);
    defaultlogging = itsConfig->get_optional_config_param("defaultlogging", defaultlogging);
    lazylinking = itsConfig->get_optional_config_param("lazylinking", lazylinking);
    accesslogdir = itsConfig->get_optional_config_param("accesslogdir", accesslogdir);

    slowpool.minsize =
        itsConfig->get_optional_config_param("slowpool.maxthreads", slowpool.minsize);
    slowpool.maxrequeuesize =
        itsConfig->get_optional_config_param("slowpool.maxrequeuesize", slowpool.maxrequeuesize);

    fastpool.minsize =
        itsConfig->get_optional_config_param("fastpool.maxthreads", fastpool.minsize);
    fastpool.maxrequeuesize =
        itsConfig->get_optional_config_param("fastpool.maxrequeuesize", fastpool.maxrequeuesize);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    std::cout << ANSI_ITALIC_ON << "Starting with the following settings:" << std::endl
              << "Debug mode\t\t\t= " << (debug ? "ON" : "OFF") << std::endl
              << "Quiet mode\t\t\t= " << (quiet ? "ON" : "OFF") << std::endl
              << "Log requests\t\t\t= " << (logrequests ? "ON" : "OFF") << std::endl
              << "Config file\t\t\t= " << configfile << std::endl
              << "Max fast threads\t\t= " << fastpool.minsize << std::endl
              << "Max fast queue size\t\t= " << fastpool.maxrequeuesize << std::endl
              << "Max slow threads\t\t= " << slowpool.minsize << std::endl
              << "Max slow queue size\t\t= " << slowpool.maxrequeuesize << std::endl
              << "Port\t\t\t\t= " << port << std::endl
              << "Timeout\t\t\t\t= " << timeout << std::endl
              << "Access log directory\t\t= " << accesslogdir << std::endl
              << "Logs requests by default\t= " << defaultlogging << ANSI_ITALIC_OFF << std::endl
              << "Lazy linking\t\t\t= " << (lazylinking ? "ON" : "OFF") << std::endl
              << std::endl;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
