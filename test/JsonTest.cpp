// ======================================================================
/*!
 * \file
 * \brief Regression tests for namespace Json
 */
// ======================================================================

#include "HTTP.h"
#include "Json.h"
#include <dtl/dtl.hpp>
#include <regression/tframe.h>

//! Protection against conflicts with global functions
namespace JsonTest
{
const char* text1 = R"EOF({
     "producer": "pal_skandinavia",
     "language": "fi",
     "projection":
     {
         "crs": "data",
         "xsize": 500
     },
     "projectionglob":
     {
         "qid": "",
         "crs": "data",
         "xsize": 500
     },
     "views": [
        {
          "attributes":
          {
             "qid": "main",
             "filter": "shadow"
          }	,
        "layers": [
         {
            "qid": "layer1",
            "layer_type": "isoband",
            "parameter": "Temperature",
            "attributes":
            {
               "fill": "red"
            }
         }
        ]
       }
     ]
  }
)EOF";

// ----------------------------------------------------------------------

void jsoncpp()
{
  Json::Value json;
  Json::Reader reader;

  if (!reader.parse(text1, json))
    TEST_FAILED("Failed to parse the sample json");

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void expand()
{
  Json::Value json;
  Json::Reader reader;

  if (!reader.parse(text1, json))
    TEST_FAILED("Failed to parse the sample json");

  SmartMet::Spine::HTTP::ParamMap params = {{"time", "2014-09-05"},
                                            {"language", "en"},
                                            {"projection.xsize", "666"},
                                            {"projection.ysize", "666"},
                                            {"bbox", "1,2,3,4"},
                                            {"crs", "WGS84"},
                                            {"ysize", "777"},
                                            {"main.filter", "emboss"},
                                            {"main.fill", "none"},
                                            {"layer1.parameter", "RoadTemperature"},
                                            {"layer1.attributes.fill", "none"},
                                            {"layer1.attributes.stroke", "red"},
                                            {"layer1.extra", "{}"},
                                            {"layer1.extra.test", "bar"},
                                            {"layer1.extra.json", "json:foo"},
                                            {"layer1.extra.ref", "ref:bar"}};

  SmartMet::Spine::JSON::expand(json, params);

  std::ostringstream out;
  out.str("");
  out << json;

  const char* expected = R"EOF({
	"bbox" : "1,2,3,4",
	"crs" : "WGS84",
	"language" : "en",
	"producer" : "pal_skandinavia",
	"projection" : 
	{
		"crs" : "data",
		"xsize" : 666,
		"ysize" : 666
	},
	"projectionglob" : 
	{
		"bbox" : "1,2,3,4",
		"crs" : "WGS84",
		"language" : "en",
		"qid" : "",
		"time" : "2014-09-05",
		"xsize" : 500,
		"ysize" : 777
	},
	"time" : "2014-09-05",
	"views" : 
	[
		{
			"attributes" : 
			{
				"fill" : "none",
				"filter" : "emboss",
				"qid" : "main"
			},
			"layers" : 
			[
				{
					"attributes" : 
					{
						"fill" : "none",
						"stroke" : "red"
					},
					"extra" : 
					{
						"test" : "bar"
					},
					"layer_type" : "isoband",
					"parameter" : "RoadTemperature",
					"qid" : "layer1"
				}
			]
		}
	],
	"ysize" : 777
})EOF";

  std::string result = out.str();

  if (result != expected)
  {
    std::vector<std::string> expected_lines;
    std::vector<std::string> result_lines;

    boost::algorithm::split(expected_lines, expected, boost::algorithm::is_any_of("\n"));
    boost::algorithm::split(result_lines, result, boost::algorithm::is_any_of("\n"));

    dtl::Diff<std::string> d(expected_lines, result_lines);

    d.compose();
    d.composeUnifiedHunks();
    std::ostringstream tmp;
    d.printUnifiedFormat(tmp);
    std::string ret = tmp.str();

    TEST_FAILED("Incorrect result:\n" + ret);
  }

  TEST_PASSED();
}

// ----------------------------------------------------------------------

void replaceReferences()
{
  Json::Value json;
  Json::Reader reader;

  if (!reader.parse(text1, json))
    TEST_FAILED("Failed to parse the sample json");

  SmartMet::Spine::HTTP::ParamMap params = {{"time", "2014-09-05"},
                                            {"language", "en"},
                                            {"projection.xsize", "666"},
                                            {"projection.ysize", "666"},
                                            {"bbox", "1,2,3,4"},
                                            {"crs", "WGS84"},
                                            {"ysize", "777"},
                                            {"main.filter", "emboss"},
                                            {"main.fill", "none"},
                                            {"layer1.parameter", "RoadTemperature"},
                                            {"layer1.json", "json:foo"},
                                            {"layer1.ref", "ref:bar"}};

  SmartMet::Spine::JSON::replaceReferences(json, params);

  std::ostringstream out;
  out.str("");
  out << json;

  const char* expected = R"EOF({
	"language" : "fi",
	"producer" : "pal_skandinavia",
	"projection" : 
	{
		"crs" : "data",
		"xsize" : 500
	},
	"projectionglob" : 
	{
		"crs" : "data",
		"qid" : "",
		"xsize" : 500
	},
	"views" : 
	[
		{
			"attributes" : 
			{
				"filter" : "shadow",
				"qid" : "main"
			},
			"layers" : 
			[
				{
					"attributes" : 
					{
						"fill" : "red"
					},
					"json" : "json:foo",
					"layer_type" : "isoband",
					"parameter" : "Temperature",
					"qid" : "layer1",
					"ref" : "ref:bar"
				}
			]
		}
	]
})EOF";

  std::string result = out.str();

  if (result != expected)
  {
    std::vector<std::string> expected_lines;
    std::vector<std::string> result_lines;

    boost::algorithm::split(expected_lines, expected, boost::algorithm::is_any_of("\n"));
    boost::algorithm::split(result_lines, result, boost::algorithm::is_any_of("\n"));

    dtl::Diff<std::string> d(expected_lines, result_lines);

    d.compose();
    d.composeUnifiedHunks();
    std::ostringstream tmp;
    d.printUnifiedFormat(tmp);
    std::string ret = tmp.str();

    TEST_FAILED("Incorrect result:\n" + ret);
  }

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
    TEST(jsoncpp);
    TEST(expand);
    TEST(replaceReferences);
  }
};

}  // namespace JsonTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "Json tester" << endl << "===========" << endl;
  JsonTest::tests t;
  return t.run();
}

// ======================================================================
