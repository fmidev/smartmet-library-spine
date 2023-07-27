// ======================================================================
/*!
 * \brief Validation tool for configuration files
 */
// ======================================================================

#include <macgyver/CsvReader.h>
#include <newbase/NFmiSettings.h>

#include <libconfig.h++>

#include <boost/filesystem/operations.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include <iostream>
#include <string>

// ----------------------------------------------------------------------
/*!
 * \brief Dummy callback for CsvReader
 */
// ----------------------------------------------------------------------

void csvhandler(const Fmi::CsvReader::row_type& row) {}

// ----------------------------------------------------------------------
/*!
 * \brief Container for command line options
 */
// ----------------------------------------------------------------------

struct Options
{
  Options();

  bool verbose;
  std::string configtype;  // libconfig, newbase, csv
  std::string configfile;
};

Options options;

// ----------------------------------------------------------------------
/*!
 * \brief Default options
 */
// ----------------------------------------------------------------------

Options::Options() : verbose(false), configtype("libconfig"), configfile() {}

// ----------------------------------------------------------------------
/*!
 * \brief Parse command line options
 *
 * \return True, if execution may continue as usual
 */
// ----------------------------------------------------------------------

bool parse_options(int argc, char* argv[], Options& options)
{
  namespace po = boost::program_options;
  namespace fs = boost::filesystem;

  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "print out help message")(
      "verbose,v",
      po::bool_switch(&options.verbose),
      "set verbose mode on")("version,V",
                             "display version number")("type,t",
                                                       po::value(&options.configtype),
                                                       "configuration type "
                                                       "(libconfig|newbase|csv)")("file,f",
                                                                                  po::value(
                                                                                      &options
                                                                                           .configfile),
                                                                                  "configuration "
                                                                                  "file to be "
                                                                                  "validated");

  po::positional_options_description p;
  p.add("file", 1);

  po::variables_map opt;
  po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), opt);

  po::notify(opt);

  if (opt.count("version") != 0)
  {
    std::cout << "cfgvalidate v2.0 (" << __DATE__ << ' ' << __TIME__ << ')' << std::endl;
  }

  if (opt.count("help"))
  {
    std::cout << "Usage: cfgvalidate [options] configfile" << std::endl
              << std::endl
              << "Return value of cfgvalidate is zero if the configuration file" << std::endl
              << "validates, otherwise one." << std::endl
              << std::endl
              << desc << std::endl;
    return false;
  }

  if (opt.count("file") == 0)
    throw std::runtime_error("Expecting configuration file as parameter 1");

  if (!fs::exists(options.configfile))
    throw std::runtime_error("Configuration file '" + options.configfile + "' does not exist");

  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program without exception handling
 */
// ----------------------------------------------------------------------

int run(int argc, char* argv[])
{
  if (!parse_options(argc, argv, options))
    return 0;

  if (options.verbose)
  {
    std::cout << "File:  " << options.configfile << std::endl
              << "Type:  " << options.configtype << std::endl;
  }

  if (options.configtype == "libconfig")
  {
    libconfig::Config config;
    try
    {
      config.readFile(options.configfile.c_str());
    }
    catch (libconfig::ParseException& e)
    {
      throw std::runtime_error(std::string("Configuration error ' ") + e.getError() + "' on line " +
                               std::to_string(e.getLine()));
    }
    catch (libconfig::FileIOException& e)
    {
      throw std::runtime_error("I/O error while reading file");
    }
  }
  else if (options.configtype == "newbase")
  {
    if (!NFmiSettings::Read(options.configfile))
      throw std::runtime_error("Failed to parse configuration file");
  }
  else if (options.configtype == "csv")
  {
    Fmi::CsvReader::read(options.configfile, &csvhandler);
  }
  else
  {
    throw std::runtime_error("Unknown configuration type '" + options.configtype + "'");
  }

  if (options.verbose)
  {
    std::cout << "Result: OK" << std::endl;
  }

  return 0;
}

// ----------------------------------------------------------------------
/*!
 * \brief Main program with exception handling
 */
// ----------------------------------------------------------------------

int main(int argc, char* argv[])
{
  try
  {
    return run(argc, argv);
  }
  catch (std::exception& e)
  {
    std::cout << "Error: " << e.what() << std::endl;
    return 1;
  }
  catch (...)
  {
    std::cout << "Error: Caught an unknown exception" << std::endl;
    return 1;
  }
}
