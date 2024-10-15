#include "Parameters.h"

namespace SmartMet
{
namespace Spine
{
namespace Parameters
{
bool IsMetaParameter(FmiParameterName param)
{
  if (IsDataIndependent(param))
    return true;
  if (IsDataDerived(param))
    return true;
  return false;
}

bool IsDataIndependent(FmiParameterName param)
{
  switch (param)
  {
    case kFmiLocalTZ:
    case kFmiCountry:
    case kFmiCoverType:
    case kFmiDEM:
    case kFmiDark:
    case kFmiDayLength:
    case kFmiDirection:
    case kFmiDistance:
    case kFmiElevation:
    case kFmiEpochTime:
    case kFmiFMISID:
    case kFmiFeature:
    case kFmiGEOID:
    case kFmiGridNorth:
    case kFmiHour:
    case kFmiISO2:
    case kFmiISOTime:
    case kFmiLPNN:
    case kFmiLevel:
    case kFmiLocalTime:
    case kFmiModTime:
    case kFmiModel:
    case kFmiMon:
    case kFmiMonth:
    case kFmiMoonDown24h:
    case kFmiMoonPhase:
    case kFmiMoonUp24h:
    case kFmiMoonrise2:
    case kFmiMoonrise2Today:
    case kFmiMoonrise:
    case kFmiMoonriseToday:
    case kFmiMoonset2:
    case kFmiMoonset2Today:
    case kFmiMoonset:
    case kFmiMoonsetToday:
    case kFmiName:
    case kFmiNearLatLon:
    case kFmiNearLatitude:
    case kFmiNearLonLat:
    case kFmiNearLongitude:
    case kFmiNoon:
    case kFmiOriginTime:
    case kFmiPlace:
    case kFmiPopulation:
    case kFmiProducer:
    case kFmiRWSID:
    case kFmiRegion:
    case kFmiSensorNo:
    case kFmiStationElevation:
    case kFmiStationLatitude:
    case kFmiStationLongitude:
    case kFmiStationName:
    case kFmiStationType:
    case kFmiStationary:
    case kFmiSunAzimuth:
    case kFmiSunDeclination:
    case kFmiSunElevation:
    case kFmiSunrise:
    case kFmiSunriseToday:
    case kFmiSunset:
    case kFmiSunsetToday:
    case kFmiTZ:
    case kFmiTime:
    case kFmiTimeString:
    case kFmiUTCTime:
    case kFmiWDay:
    case kFmiWmoStationNumber:
    case kFmiWeekday:
    case kFmiWSI:
    case kFmiXMLTime:
      return true;
    default:
      return false;
  }
}

bool IsDataDerived(FmiParameterName param)
{
  switch (param)
  {
    case kFmiApparentTemperature:
    case kFmiCloudiness8th:
    case kFmiDataSource:
    case kFmiFeelsLike:
    case kFmiCloudCeilingHFT:
    case kFmiLatLon:
    case kFmiLatitude:
    case kFmiLonLat:
    case kFmiLongitude:
    case kFmiSmartSymbol:
    case kFmiSmartSymbolText:
    case kFmiSnow1h:
    case kFmiSnow1hLower:
    case kFmiSnow1hUpper:
    case kFmiSummerSimmerIndex:
    case kFmiWeather:
    case kFmiWeatherNumber:
    case kFmiWeatherSymbol:
    case kFmiWindChill:
    case kFmiWindCompass16:
    case kFmiWindCompass32:
    case kFmiWindCompass8:
    case kFmiWindUMS:
    case kFmiWindVMS:
      return true;
    default:
      return false;
  }
}

}  // namespace Parameters
}  // namespace Spine
}  // namespace SmartMet
