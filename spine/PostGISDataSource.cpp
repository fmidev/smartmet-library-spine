#include "PostGISDataSource.h"
#include "Exception.h"

#include <macgyver/StringConversion.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/foreach.hpp>

#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;
using namespace boost;

namespace SmartMet
{
namespace Spine
{
bool PostGISDataSource::readData(const postgis_identifier& postGISIdentifier,
                                 std::string& log_message)
{
  try
  {
    return readData(postGISIdentifier.postGISHost,
                    postGISIdentifier.postGISPort,
                    postGISIdentifier.postGISDatabase,
                    postGISIdentifier.postGISUsername,
                    postGISIdentifier.postGISPassword,
                    postGISIdentifier.postGISSchema,
                    postGISIdentifier.postGISTable,
                    postGISIdentifier.postGISField,
                    postGISIdentifier.postGISClientEncoding,
                    log_message);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool PostGISDataSource::readData(const std::string& host,
                                 const std::string& port,
                                 const std::string& dbname,
                                 const std::string& user,
                                 const std::string& password,
                                 const std::string& schema,
                                 const std::string& table,
                                 const std::string& fieldname,
                                 const std::string& client_encoding,
                                 std::string& log_message)
{
  try
  {
    std::string queryparameter(host + port + dbname + schema + table + fieldname + client_encoding);
    if (queryparametermap.find(queryparameter) != queryparametermap.end())
      return true;

    std::stringstream connection_ss;

    connection_ss << "PG:host='" << host << "' port='" << port << "' dbname='" << dbname
                  << "' user='" << user << "' password='" << password << "'";

    OGRRegisterAll();

    OGRDataSource* pDS = OGRSFDriverRegistrar::Open(connection_ss.str().c_str(), FALSE);

    if (!pDS)
    {
      throw SmartMet::Spine::Exception(
          BCP, "Error: OGRSFDriverRegistrar::Open(" + connection_ss.str() + ") failed!");
    }

    std::string sqlstmt("SET CLIENT_ENCODING TO '" + client_encoding + "'");
    pDS->ExecuteSQL(sqlstmt.c_str(), 0, 0);

    std::stringstream schema_table_ss;

    schema_table_ss << schema << "." << table;

    OGRLayer* pLayer = pDS->GetLayerByName(schema_table_ss.str().c_str());

    if (pLayer == NULL)
    {
      throw SmartMet::Spine::Exception(
          BCP, "Error: OGRDataSource::GetLayerByName(" + schema_table_ss.str() + ") failed!");
    }

    // get spatial reference
    OGRSpatialReference* pLayerSRS = pLayer->GetSpatialRef();
    OGRCoordinateTransformation* pCoordinateTransform(0);
    OGRSpatialReference targetTransformSRS;
    if (pLayerSRS)
    {
#ifdef MYDEBUG
      int UTMZone(0);
      UTMZone = pLayerSRS->GetUTMZone();
      cout << "UTMZone: " << UTMZone << endl;
      cout << "IsGeographic: " << pLayerSRS->IsGeographic() << endl;
      cout << "IsProjected: " << pLayerSRS->IsProjected() << endl;
      char* wkt_buffer(0);
      pLayerSRS->exportToPrettyWkt(&wkt_buffer);
      cout << wkt_buffer << endl;
      CPLFree(wkt_buffer);
#endif

      // set WGS84 coordinate system
      targetTransformSRS.SetWellKnownGeogCS("WGS84");

      // create transformation object
      pCoordinateTransform = OGRCreateCoordinateTransformation(pLayerSRS, &targetTransformSRS);
    }

    OGRFeature* pFeature(0);
    pLayer->ResetReading();

    while ((pFeature = pLayer->GetNextFeature()) != NULL)
    {
      OGRFeatureDefn* pFDefn = pLayer->GetLayerDefn();

      // find name for the area
      std::string area_name("");
      int iField;
      for (iField = 0; iField < pFDefn->GetFieldCount(); iField++)
      {
        OGRFieldDefn* pFieldDefn = pFDefn->GetFieldDefn(iField);

        if (fieldname.compare(pFieldDefn->GetNameRef()) == 0)
        {
          area_name = pFeature->GetFieldAsString(iField);
          break;
        }
      }

      if (area_name.empty())
      {
        log_message = "field " + fieldname + " not found for the feature " + pFDefn->GetName();
        continue;
      }

      // convert to lower case to make case insensitive queries easy
      boost::algorithm::to_lower(area_name);

      // get geometry
      OGRGeometry* pGeometry(pFeature->GetGeometryRef());

      if (pGeometry)
      {
        if (pLayerSRS && pCoordinateTransform)
        {
          // transform the coordinates to wgs84
          if (OGRERR_NONE != pGeometry->transform(pCoordinateTransform))
            log_message = "pGeometry->transform() failed";
        }

        OGRwkbGeometryType geometryType =
            static_cast<OGRwkbGeometryType>(pGeometry->getGeometryType() & (~wkb25DBit));

        if (geometrymap.find(geometryType) == geometrymap.end())
        {
          // if that type of geometries not found, add new one
          NameOGRGeometryMap nameOGRGeometryMap;
          nameOGRGeometryMap.insert(
              make_pair(area_name, boost::shared_ptr<OGRGeometry>(pGeometry->clone())));
          geometrymap.insert(make_pair(geometryType, nameOGRGeometryMap));
        }
        else
        {
          // that type of geometries found
          NameOGRGeometryMap& nameOGRGeometryMap = geometrymap[geometryType];
          // if named area not found, add new one
          if (nameOGRGeometryMap.find(area_name) == nameOGRGeometryMap.end())
          {
            nameOGRGeometryMap.insert(
                make_pair(area_name, boost::shared_ptr<OGRGeometry>(pGeometry->clone())));
          }
          else
          {
            // if area was found, make union of the new and old one
            boost::shared_ptr<OGRGeometry>& geom(nameOGRGeometryMap[area_name]);
            geom.reset(geom->Union(pGeometry));
          }
        }

        if (geometryType == wkbPoint)
        {
          OGRPoint* pPoint = reinterpret_cast<OGRPoint*>(pGeometry);
          if (pointmap.find(area_name) != pointmap.end())
            pointmap[area_name] = make_pair(pPoint->getX(), pPoint->getY());
          else
            pointmap.insert(make_pair(area_name, make_pair(pPoint->getX(), pPoint->getY())));
        }
        else if (geometryType == wkbMultiPolygon || geometryType == wkbPolygon)
        {
          string svg_string("");
          if (geometryType == wkbMultiPolygon)
          {
            OGRMultiPolygon* pMultiPolygon = reinterpret_cast<OGRMultiPolygon*>(pGeometry);
            char* wkt_buffer(0);
            pMultiPolygon->exportToWkt(&wkt_buffer);
            svg_string.append(wkt_buffer);
#ifdef MYDEBUG
            std::cout << "WKT MULTIPOLYGON: " << std::string(wkt_buffer) << std::endl;
#endif

            CPLFree(wkt_buffer);
          }
          else
          {
            OGRPolygon* pPolygon = reinterpret_cast<OGRPolygon*>(pGeometry);

            char* wkt_buffer(0);
            pPolygon->exportToWkt(&wkt_buffer);
            svg_string.append(wkt_buffer);
#ifdef MYDEBUG
            std::cout << "WKT POLYGON: " << std::string(wkt_buffer) << std::endl;
#endif
            CPLFree(wkt_buffer);
          }

          replace_all(svg_string, "MULTIPOLYGON ", "");
          replace_all(svg_string, "POLYGON ", "");
          replace_all(svg_string, "),(", " Z M ");
          replace_all(svg_string, ",", " L ");
          replace_all(svg_string, "(", "");
          replace_all(svg_string, ")", "");
          svg_string.insert(0, "\"M ");
          svg_string.append(" Z\"\n");

#ifdef MYDEBUG
          cout << "POLYGON in SVG format: " << svg_string << endl;
#endif
          if (polygonmap.find(area_name) != polygonmap.end())
            polygonmap[area_name] = svg_string;
          else
            polygonmap.insert(make_pair(area_name, svg_string));
        }
        else if (geometryType == wkbMultiLineString || geometryType == wkbLineString)
        {
          string svg_string("");
          if (geometryType == wkbMultiLineString)
          {
            OGRMultiLineString* pMultiLine = reinterpret_cast<OGRMultiLineString*>(pGeometry);

            char* wkt_buffer(0);
            pMultiLine->exportToWkt(&wkt_buffer);
            svg_string.append(wkt_buffer);
            CPLFree(wkt_buffer);
          }
          else
          {
            OGRLineString* pLine = reinterpret_cast<OGRLineString*>(pGeometry);

            char* wkt_buffer(0);
            pLine->exportToWkt(&wkt_buffer);
            svg_string.append(wkt_buffer);
            CPLFree(wkt_buffer);
#ifdef MYDEBUG
            cout << "LINESTRING: " << svg_string << endl;
#endif
          }

          replace_all(svg_string, "MULTILINESTRING ", "");
          replace_all(svg_string, "LINESTRING ", "");
          replace_all(svg_string, "))((", ",");
          replace_all(svg_string, ",", " L ");
          replace_all(svg_string, "(", "");
          replace_all(svg_string, ")", "");
          svg_string.append(" \"\n");

          if (linemap.find(area_name) != linemap.end())
          {
            string previous_part(linemap[area_name]);
            replace_all(previous_part, " \"", " ");
            replace_all(previous_part, " \n", " ");
            //						replace_all(previous_part, "M", "L");
            svg_string = (previous_part + "L " + svg_string);
          }
          else
          {
            svg_string.insert(0, "\"M ");
          }

#ifdef MYDEBUG
          cout << "LINE in SVG format: " << svg_string << endl;
#endif

          if (linemap.find(area_name) != linemap.end())
            linemap[area_name] = svg_string;
          else
            linemap.insert(make_pair(area_name, svg_string));
        }
        else
        {
          // no other geometries handled
        }
      }
      // destroy feature
      OGRFeature::DestroyFeature(pFeature);
    }

    if (pCoordinateTransform)
    {
      delete pCoordinateTransform;
    }

    // in the end destroy data source
    OGRDataSource::DestroyDataSource(pDS);

    queryparametermap.insert(make_pair(queryparameter, 1));

    return true;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string PostGISDataSource::getSVGPath(const std::string& name) const
{
  try
  {
    std::string key = boost::algorithm::to_lower_copy(name);

    if (polygonmap.find(key) != polygonmap.end())
      return polygonmap.at(key);
    else if (linemap.find(key) != linemap.end())
      return linemap.at(key);
    else
      return "";
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::pair<double, double> PostGISDataSource::getPoint(const std::string& name) const
{
  try
  {
    std::string key = boost::algorithm::to_lower_copy(name);

    if (pointmap.find(key) != pointmap.end())
      return pointmap.at(key);
    else
      return make_pair(32700.0, 32700.0);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

const OGRGeometry* PostGISDataSource::getOGRGeometry(const std::string& name,
                                                     OGRwkbGeometryType type) const
{
  try
  {
    OGRGeometry* ret(0);

    std::string key = boost::algorithm::to_lower_copy(name);

    if (geometrymap.find(type) != geometrymap.end())
    {
      const NameOGRGeometryMap& nameOGRGeometryMap = geometrymap.find(type)->second;

      if (nameOGRGeometryMap.find(key) != nameOGRGeometryMap.end())
      {
        boost::shared_ptr<OGRGeometry> g = nameOGRGeometryMap.find(key)->second;
        ret = g.get();
      }
    }

    return ret;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

OGRDataSource* PostGISDataSource::connect(const std::string& host,
                                          const std::string& port,
                                          const std::string& dbname,
                                          const std::string& user,
                                          const std::string& password)
{
  try
  {
    OGRRegisterAll();

    OGRSFDriver* pOGRDriver(OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName("PostgreSQL"));

    if (!pOGRDriver)
    {
      throw SmartMet::Spine::Exception(BCP, "Error: PostgreSQL driver not found!");
    }

    std::stringstream ss;

    ss << "PG:host='" << host << "' port='" << port << "' dbname='" << dbname << "' user='" << user
       << "' password='" << password << "'";

    return pOGRDriver->Open(ss.str().c_str());
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool PostGISDataSource::geoObjectExists(const std::string& name) const
{
  try
  {
    std::string key = boost::algorithm::to_lower_copy(name);

    return (isPolygon(key) || isLine(key) || isPoint(key));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool PostGISDataSource::isPolygon(const std::string& name) const
{
  try
  {
    return polygonmap.find(boost::algorithm::to_lower_copy(name)) != polygonmap.end();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool PostGISDataSource::isLine(const std::string& name) const
{
  try
  {
    return linemap.find(boost::algorithm::to_lower_copy(name)) != linemap.end();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool PostGISDataSource::isPoint(const std::string& name) const
{
  try
  {
    return pointmap.find(boost::algorithm::to_lower_copy(name)) != pointmap.end();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::list<string> PostGISDataSource::areaNames() const
{
  try
  {
    std::list<string> return_list;
    typedef std::map<std::string, std::string> polygonmap_t;
    typedef std::map<std::string, std::pair<double, double> > pointmap_t;

    BOOST_FOREACH (const polygonmap_t::value_type& vt, polygonmap)
    {
      return_list.push_back(vt.first);
    }
    BOOST_FOREACH (const pointmap_t::value_type& vt, pointmap)
    {
      return_list.push_back(vt.first);
    }

    return return_list;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Spine
}  // namespace SmartMet
