// ======================================================================
/*!
 * \file
 * \brief Regression tests for class CsvFormatter
 */
// ======================================================================

#include "CsvFormatter.h"
#include "HTTP.h"
#include "Table.h"
#include "TableFormatterOptions.h"
#include <regression/tframe.h>
#include <cmath>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

template <typename T>
std::string tostr(const T& theValue)
{
  std::ostringstream out;
  out << theValue;
  return out.str();
}

SmartMet::Spine::TableFormatterOptions config;

//! Protection against conflicts with global functions
namespace CsvFormatterTest
{
// ----------------------------------------------------------------------

void format()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names{"a", "b", "c", "d"};

  tab.set(0, 0, "0");
  tab.set(1, 2, "12");
  tab.set(3, 1, "31");
  tab.set(2, 1, "");

  const char* res = R"("a","b","c","d"
0,"NaN","NaN","NaN"
"NaN","NaN","NaN",31
"NaN",12,"NaN","NaN"
)";

  SmartMet::Spine::HTTP::Request req;

  SmartMet::Spine::CsvFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result:\n" + out + "Expected result:\n" + res);

  TEST_PASSED();
}

void format_names_from_table()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names{"a", "b", "c", "d"};
  tab.setNames(names);

  tab.set(0, 0, "0");
  tab.set(1, 2, "12");
  tab.set(3, 1, "31");
  tab.set(2, 1, "");

  const char* res = R"("a","b","c","d"
0,"NaN","NaN","NaN"
"NaN","NaN","NaN",31
"NaN",12,"NaN","NaN"
)";

  SmartMet::Spine::HTTP::Request req;

  SmartMet::Spine::CsvFormatter fmt;
  auto out = fmt.format(tab, {}, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result:\n" + out + "Expected result:\n" + res);

  TEST_PASSED();
}

void format_partial_names_from_table()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names{"a", "b"};
  tab.setNames(names);

  tab.set(0, 0, "0");
  tab.set(1, 2, "12");
  tab.set(3, 1, "31");
  tab.set(2, 1, "");

  const char* res = R"("a","b","",""
0,"NaN","NaN","NaN"
"NaN","NaN","NaN",31
"NaN",12,"NaN","NaN"
)";

  SmartMet::Spine::HTTP::Request req;

  SmartMet::Spine::CsvFormatter fmt;
  auto out = fmt.format(tab, {}, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result:\n" + out + "Expected result:\n" + res);

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void missingtext()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names{"a", "b", "c", "d"};

  tab.set(0, 0, "0");
  tab.set(1, 2, "12");
  tab.set(3, 1, "31");
  tab.set(2, 1, "");

  const char* res = R"("a","b","c","d"
0,"-","-","-"
"-","-","-",31
"-",12,"-","-"
)";

  SmartMet::Spine::HTTP::Request req;
  req.setParameter("missingtext", "-");

  SmartMet::Spine::CsvFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result:\n" + out + "Expected result:\n" + res);

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * \brief Test formatting an empty table
 */
// ----------------------------------------------------------------------

void empty()
{
  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names{"a", "b", "c", "d"};
  SmartMet::Spine::HTTP::Request req;

  const char* res = R"("a","b","c","d"
)";

  SmartMet::Spine::CsvFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result:\n" + out + "Expected result:\n" + res);

  TEST_PASSED();
}

void number_detection()
{
  // Numbers are output verbatim without quoting. CSV has no number grammar of
  // its own, hence the values are not normalized like they are for JSON. The
  // special values NaN and Inf are not numbers and are hence quoted.
  const std::vector<std::pair<std::string, std::string>> values = {{"123", "123"},
                                                                   {"-45.67", "-45.67"},
                                                                   {"+1", "+1"},
                                                                   {"007", "007"},
                                                                   {".5", ".5"},
                                                                   {"5.", "5."},
                                                                   {"12345E-4", "12345E-4"},
                                                                   {"Nan", "\"Nan\""},
                                                                   {"NaN", "\"NaN\""},
                                                                   {"nan", "\"nan\""},
                                                                   {"Inf", "\"Inf\""},
                                                                   {"-Inf", "\"-Inf\""},
                                                                   {"infinity", "\"infinity\""},
                                                                   {"abc", "\"abc\""},
                                                                   {"123x", "\"123x\""}};

  SmartMet::Spine::Table tab;
  SmartMet::Spine::TableFormatter::Names names;

  std::string header;
  std::string row;
  for (std::size_t i = 0; i < values.size(); ++i)
  {
    const std::string name = "n" + std::to_string(i);
    names.push_back(name);
    tab.set(i, 0, values[i].first);

    if (i > 0)
    {
      header += ',';
      row += ',';
    }
    header += "\"" + name + "\"";
    row += values[i].second;
  }

  const std::string res = header + "\n" + row + "\n";

  SmartMet::Spine::HTTP::Request req;

  SmartMet::Spine::CsvFormatter fmt;
  auto out = fmt.format(tab, names, req, config);

  if (out != res)
    TEST_FAILED("Incorrect result:\n" + out + "Expected result:\n" + res);

  TEST_PASSED();
}

// ----------------------------------------------------------------------
/*!
 * The actual test suite
 */
// ----------------------------------------------------------------------

class tests : public tframe::tests
{
  virtual const char* error_message_prefix() const { return "\n\t"; }
  void test(void)
  {
    TEST(format);
    TEST(format_names_from_table);
    TEST(format_partial_names_from_table);
    TEST(missingtext);
    TEST(number_detection);
    TEST(empty);
  }
};

}  // namespace CsvFormatterTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "CsvFormatter tester" << endl << "====================" << endl;
  CsvFormatterTest::tests t;
  return t.run();
}
