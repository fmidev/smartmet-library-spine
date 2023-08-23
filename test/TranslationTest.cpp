// ======================================================================
/*!
 * \file
 * \brief Regression tests for class Translations
 */
// ======================================================================

#include "Translations.h"
#include <regression/tframe.h>

//! Protection against conflicts with global functions
namespace TranslationTest
{
// ----------------------------------------------------------------------

// ----------------------------------------------------------------------
/*!
 * \brief Test translations
 */
// ----------------------------------------------------------------------

void translations()
{
   libconfig::Config config;
   config.readFile("cnf/translations.conf");
   SmartMet::Spine::Translations tr(config);
   auto weather_fi = tr.getParameterTranslation("weathertext", 3, "fi");
   auto weather_sv = tr.getParameterTranslation("weathertext", 3, "sv");
   auto weather_en = tr.getParameterTranslation("weathertext", 3, "en");
   if(!weather_fi)
	 weather_fi = "none";
   if(!weather_sv)
	 weather_sv = "none";
   if(!weather_en)
	 weather_en = "none";
   if (*weather_fi != "pilvistä")
	 TEST_FAILED("Incorrect result for weathertext parameter value 3 in finnish:\n" + *weather_fi);
   if (*weather_sv != "mulet")
	 TEST_FAILED("Incorrect result for weathertext parameter value 3 in swedish:\n" + *weather_sv);
   if (*weather_en != "cloudy")
    TEST_FAILED("Incorrect result for weathertext parameter value 3 in english:\n" + *weather_en);

   auto provider_fi = tr.getStringTranslation("providername", "fi");
   auto provider_en = tr.getStringTranslation("providername", "en");
   if(!provider_fi)
	 provider_fi = "none";
   if(!provider_en)
	 provider_en = "none";
   if (*provider_fi != "Ilmatieteen laitos")
	 TEST_FAILED("Incorrect result for string 'providename' in finnish:\n" + *provider_fi);
   if (*provider_en != "Finnish Meteorological Institute")
    TEST_FAILED("Incorrect result for string 'providername' in english:\n" + *provider_en);

   auto keywords_fi = tr.getStringArrayTranslation("keywords", "fi");
   auto keywords_en = tr.getStringArrayTranslation("keywords", "en");
   std::string keywords_str_fi = "none";
   std::string keywords_str_en = "none";
   if(keywords_fi)
	keywords_str_fi = std::accumulate(keywords_fi->begin(), keywords_fi->end(), std::string(""));
   if(keywords_en)
	keywords_str_en = std::accumulate(keywords_en->begin(), keywords_en->end(), std::string(""));
   if (keywords_str_fi != "SääMeriTutkaHavaintoEnnusteEnnustemalliHirlamSäteilyRadioaktiivisuusIlmanlaatu")
	 TEST_FAILED("Incorrect result for keywords in finnish:\n" + keywords_str_fi);
   if (keywords_str_en != "WeatherOceanRadarObservationForecastModelHirlamRadiationRadioactivityAirquality")
	 TEST_FAILED("Incorrect result for keywords in english:\n" + keywords_str_en);

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
    TEST(translations);
  }
};

}  // namespace TranstationTest

//! The main program
int main(void)
{
  using namespace std;
  cout << endl << "Translations tester" << endl << "=====================" << endl;
  TranslationTest::tests t;
  return t.run();
}

// ======================================================================
