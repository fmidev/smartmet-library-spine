// ======================================================================
/*!
 * \brief Implementation of class HighChartsFormatter
 *
 * Sample requests:
 *
 * 1) Linechart of temperature in Helsinki and Rovaniemi
 * http://localhost:8885/timeseries?format=highcharts&precision=double&producer=pal&param=name,localtime,
 * t2m%20as%20l%C3%A4mp%C3%B6tila&places=Helsinki,Rovaniemi&timestep=30m&endtime=data&highcharts=charttype=linechart%23
 * xcolumn=1%23ycolumns=2%23groupbycolumn=0%23title=L%C3%A4mp%C3%B6tila%20(%C2%B0C)%23subtitle=producer:pal%23
 * ylabel=Temperature%20(%C2%B0C)
 *
 * 2) Linecharts of min, max, mean temperatures in Uusimaa region
 * http://localhost:8885/timeseries?producer=pal&format=highcharts&param=name,localtime,mean(t2m)%20as%20mean,
 * min(t2m)%20as%20min,%20max(t2m)%20as%20max&model=pal&area=Uusimaa&timestep=30m&endtime=data&highcharts=
 * charttype=linechart%23xcolumn=1%23ycolumns=2,3,4%23title=Min,Max,Mean%20Temperature%20(%C2%B0C)%20-%20Uusimaa%23
 * subtitle=producer:%20pal_skadinavia%23ylabel=Temperature%20(%C2%B0C)
 *
 * 3) Piechart of wind direction in Uusimaa region
 * http://localhost:8885/timeseries?format=highcharts&area=Uusimaa&timeformat=timestamp&precision=double&
 * param=name,time,percentage_a[22.501:67.5](winddirection)%20as%20koillinen,
 * percentage_a[67.501:112.5](winddirection)%20as%20it%C3%A4,percentage_a[112.501:157.5](winddirection)%20as%20kaakko,
 * percentage_a[157.501:202.5](winddirection)%20as%20etel%C3%A4,percentage_a[202.501:247.5](winddirection)%20as%20lounas,
 * percentage_a[247.501:292.5](winddirection)%20as%20l%C3%A4nsi,percentage_a[292.501:337.5](winddirection)%20as%20luode,
 * percentage_a[337.501:360](winddirection)%20as%20pohjoinen,percentage_a[0.0:22.5](winddirection)%20as%20pohjoinen&
 * starttime=20181028190000&endtime=20181028190000&highcharts=charttype=piechart%23
 * piecolumns=2,3,4,5,6,7,8,9,10%23title=Tuulen%20suunta%20Uudellamaalla%2028.10.2018%20klo%2019:00
 *
 */
// ======================================================================

#include "HighChartsFormatter.h"
#include "Convenience.h"
#include "Exception.h"
#include "Table.h"

namespace SmartMet
{
namespace Spine
{
#define CHARTTYPE "charttype"
#define LINECHART "linechart"
#define PIECHART "piechart"
#define XCOLUMN "xcolumn"
#define YCOLUMNS "ycolumns"
#define PIECOLUMNS "piecolumns"
#define TITLE "title"
#define SUBTITLE "subtitle"
#define YLABEL "ylabel"
#define GROUPBYCOLUMN "groupbycolumn"

using HighChartsSettings = std::map<std::string, std::string>;

namespace
{
void linechart(std::ostream& theOutput,
               const Table& theTable,
               const TableFormatter::Names& theNames,
               const HighChartsSettings& theHighChartsSettings)
{
  try
  {
    if (theHighChartsSettings.find(XCOLUMN) == theHighChartsSettings.end())
      throw Spine::Exception(BCP, "xcolumn-definition missing!");
    if (theHighChartsSettings.find(YCOLUMNS) == theHighChartsSettings.end())
      throw Spine::Exception(BCP, "ycolumns-definition missing!");

    // X-Axis column
    int xAxisColumn = Fmi::stoi(theHighChartsSettings.at(XCOLUMN));

    // Y-Axis columns
    std::vector<std::string> ycolumns;
    boost::algorithm::split(
        ycolumns, theHighChartsSettings.at(YCOLUMNS), boost::algorithm::is_any_of(","));
    if (ycolumns.size() == 0)
      throw Spine::Exception(BCP, "At least one Y-axis column must be defined!");

    const Table::Indexes cols = theTable.columns();
    const Table::Indexes rows = theTable.rows();

    // X-Axis values
    std::string xAxisValues;
    for (const auto j : rows)
    {
      if (!xAxisValues.empty())
        xAxisValues += ", ";
      xAxisValues += std::string("'" + theTable.get(xAxisColumn, j) + "'");
    }
    xAxisValues.insert(0, "[");
    xAxisValues.append("]");

    // Timeseries name -> data
    std::map<std::string, std::string> timeseries;
    if (theHighChartsSettings.find(GROUPBYCOLUMN) != theHighChartsSettings.end())
    {
      unsigned int groupByColumn = Fmi::stoi(theHighChartsSettings.at(GROUPBYCOLUMN));
      unsigned int dataColumn = Fmi::stoi(ycolumns[0]);
      for (const auto row : rows)
      {
        std::string name = theTable.get(groupByColumn, row);
        std::string value = theTable.get(dataColumn, row);
        if (timeseries.find(name) == timeseries.end())
          timeseries.insert(std::make_pair(name, value));
        else
          timeseries[name].append("," + value);
      }
      for (auto& ts : timeseries)
      {
        timeseries[ts.first].insert(0, "[");
        timeseries[ts.first].append("]");
      }
    }
    else
    {
      std::vector<int> yAxisColumns;
      for (const auto& y : ycolumns)
        yAxisColumns.push_back(Fmi::stoi(y));

      for (const auto yColumn : yAxisColumns)
      {
        if (yColumn >= static_cast<int>(theNames.size()))
          throw Spine::Exception(BCP, "Wrong column number " + Fmi::to_string(yColumn) + "!");

        std::string name = theNames[yColumn];
        std::string value;
        for (const auto row : rows)
        {
          if (!value.empty())
            value += ", ";
          value += htmlescape(theTable.get(yColumn, row));
        }
        value.insert(0, "[");
        value.append("]");
        timeseries.insert(std::make_pair(name, value));
      }
    }
    // Chart Title
    std::string chartTitle;
    if (theHighChartsSettings.find(TITLE) != theHighChartsSettings.end())
      chartTitle = theHighChartsSettings.at(TITLE);

    // Chart subtitle
    std::string chartSubTitle;
    if (theHighChartsSettings.find(SUBTITLE) != theHighChartsSettings.end())
      chartSubTitle = theHighChartsSettings.at(SUBTITLE);

    // Y-Axis Label
    std::string yAxisLabel;
    if (theHighChartsSettings.find(YLABEL) != theHighChartsSettings.end())
      yAxisLabel = theHighChartsSettings.at(YLABEL);

    // Output headers
    theOutput << "<html>\n";
    theOutput << "   <head>\n";
    theOutput << "      <title>Timeseries chart</title>\n";
    theOutput << "      <script src = "
                 "\"https://ajax.googleapis.com/ajax/libs/jquery/2.1.3/jquery.min.js\">\n";
    theOutput << "      </script>\n";
    theOutput << "      <script src = \"https://code.highcharts.com/highcharts.js\"></script> \n";
    theOutput << "   </head>\n";

    theOutput << "   <body>\n";
    theOutput << "      <div id = \"container\" style = \"width: 550px; height: 400px; margin: 0 "
                 "auto\"></div>\n";
    theOutput << "      <script language = \"JavaScript\">\n";
    theOutput << "         $(document).ready(function() {\n";
    theOutput << "        var title = {\n";
    theOutput << "           text: '";
    theOutput << chartTitle;
    theOutput << "'\n";
    theOutput << "        };\n";
    theOutput << "        var subtitle = {\n";
    theOutput << "           text: '";
    theOutput << chartSubTitle;
    theOutput << "'\n";
    theOutput << "        };\n";
    theOutput << "        var xAxis = {\n";
    theOutput << "           categories: \n";
    theOutput << xAxisValues;
    theOutput << "        };\n";
    theOutput << "        var yAxis = {\n";
    theOutput << "           title: {\n";
    theOutput << "              text: '";
    theOutput << yAxisLabel;
    theOutput << "'\n";
    theOutput << "           },\n";
    theOutput << "           plotLines: [{\n";
    theOutput << "              value: 0,\n";
    theOutput << "              width: 1,\n";
    theOutput << "              color: '#808080'\n";
    theOutput << "           }]\n";
    theOutput << "        };   \n";
    theOutput << "        var tooltip = {\n";
    theOutput << "           valueSuffix: ''\n";
    theOutput << "        }\n";
    theOutput << "        var legend = {\n";
    theOutput << "           layout: 'vertical',\n";
    theOutput << "           align: 'right',\n";
    theOutput << "           verticalAlign: 'middle',\n";
    theOutput << "           borderWidth: 0\n";
    theOutput << "        };\n";
    theOutput << "            var legend = {\n";
    theOutput << "               layout: 'vertical',\n";
    theOutput << "               align: 'right',\n";
    theOutput << "               verticalAlign: 'middle',\n";
    theOutput << "               borderWidth: 0\n";
    theOutput << "            };\n";
    theOutput << "            var series =  [\n";

    std::string tsItems;
    for (auto& ts : timeseries)
    {
      if (!tsItems.empty())
        tsItems += ", \n";
      tsItems += "{\n";
      tsItems += "              name: '";
      tsItems += ts.first;
      tsItems += "',\n";
      tsItems += "data: ";
      tsItems += ts.second;
      tsItems += "               }\n";
    }
    theOutput << tsItems;
    theOutput << "            ];\n";
    theOutput << "            var json = {};\n";
    theOutput << "            json.title = title;\n";
    theOutput << "            json.subtitle = subtitle;\n";
    theOutput << "            json.xAxis = xAxis;\n";
    theOutput << "            json.yAxis = yAxis;\n";
    theOutput << "            json.tooltip = tooltip;\n";
    theOutput << "            json.legend = legend;\n";
    theOutput << "            json.series = series;\n";
    theOutput << "            $('#container').highcharts(json);\n";
    theOutput << "         });\n";
    theOutput << "      </script>\n";
    theOutput << "   </body>\n";
    theOutput << "</html>\n";
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

void piechart(std::ostream& theOutput,
              const Table& theTable,
              const TableFormatter::Names& theNames,
              const HighChartsSettings& theHighChartsSettings)
{
  try
  {
    // Chart Title
    std::string chartTitle;
    if (theHighChartsSettings.find(TITLE) != theHighChartsSettings.end())
      chartTitle = theHighChartsSettings.at(TITLE);

    // Chart subtitle
    std::string chartSubTitle;
    if (theHighChartsSettings.find(SUBTITLE) != theHighChartsSettings.end())
      chartSubTitle = theHighChartsSettings.at(SUBTITLE);

    // Pie-slice columns
    std::vector<std::string> piecolumns;
    if (theHighChartsSettings.find(PIECOLUMNS) != theHighChartsSettings.end())
      boost::algorithm::split(
          piecolumns, theHighChartsSettings.at(PIECOLUMNS), boost::algorithm::is_any_of(","));
    if (piecolumns.size() == 0)
      throw Spine::Exception(BCP, "At least one Pie-column must be defined!");

    const Table::Indexes cols = theTable.columns();
    const Table::Indexes rows = theTable.rows();

    std::map<std::string, double> pieSlices;
    for (auto column : piecolumns)
    {
      int pieColumn = Fmi::stoi(column);
      if (pieColumn >= static_cast<int>(theNames.size()))
        throw Spine::Exception(BCP, "Wrong column number " + column + "!");
      std::string name = theNames.at(pieColumn);
      if (pieSlices.find(name) == pieSlices.end())
        pieSlices.insert(std::make_pair(name, 0.0));
      pieSlices[name] += Fmi::stod(theTable.get(pieColumn, 0));  // take always from first row
    }

    std::string pieSliceValues;
    for (const auto slice : pieSlices)
    {
      if (slice.second == 0.0)
        continue;
      if (!pieSliceValues.empty())
        pieSliceValues += ",\n";
      pieSliceValues += "['" + slice.first + "', ";
      pieSliceValues += std::to_string(slice.second) + "]";
    }
    pieSliceValues.insert(0, "[\n");
    pieSliceValues.append("]\n");

    // Output headers
    theOutput << "<html>\n";
    theOutput << "   <head>\n";
    theOutput << "      <title>Timeseries chart</title>\n";
    theOutput << "      <script src = "
                 "\"https://ajax.googleapis.com/ajax/libs/jquery/2.1.3/jquery.min.js\">\n";
    theOutput << "      </script>\n";
    theOutput << "      <script src = \"https://code.highcharts.com/highcharts.js\"></script> \n";
    theOutput << "   </head>\n";

    theOutput << "   <body>\n";
    theOutput << "      <div id = \"container\" style = \"width: 550px; height: 400px; margin: 0 "
                 "auto\"></div>\n";
    theOutput << "      <script language = \"JavaScript\">\n";
    theOutput << "         $(document).ready(function() {\n";
    theOutput << "          var chart = {\n";
    theOutput << "             plotBackgroundColor: null,\n";
    theOutput << "             plotBorderWidth: null,\n";
    theOutput << "             plotShadow: false\n";
    theOutput << "          };\n";
    theOutput << "        var title = {\n";
    theOutput << "           text: '";
    theOutput << chartTitle;
    theOutput << "'\n";
    theOutput << "        };\n";
    theOutput << "        var subtitle = {\n";
    theOutput << "           text: '";
    theOutput << chartSubTitle;
    theOutput << "'\n";
    theOutput << "        };\n";
    theOutput << "        var tooltip = {\n";
    theOutput << "           pointFormat: '{series.name}: <b>{point.percentage:.1f}%</b>'\n";
    theOutput << "        };\n";

    theOutput << "         var plotOptions = {\n";
    theOutput << "             pie: {\n";
    theOutput << "             allowPointSelect: true,\n";
    theOutput << "             cursor: 'pointer',\n";
    theOutput << "             dataLabels: {\n";
    theOutput << "                enabled: true,\n";
    theOutput << "                format : '<b>{point.name}%</b>: {point.percentage:.1f} %',\n ";
    theOutput << "                style: {\n";
    theOutput << "                   color: (Highcharts.theme && "
                 "Highcharts.theme.contrastTextColor) || 'black'\n";
    theOutput << "                   }\n";
    theOutput << "                }\n";
    theOutput << "             }\n";
    theOutput << "          };\n";
    theOutput << "         var series = [{\n";
    theOutput << "            type: 'pie',\n";
    theOutput << "            name: 'Share',\n";
    theOutput << "            data:\n";
    theOutput << pieSliceValues;
    theOutput << "          }];\n";
    theOutput << "          var json = {};\n";
    theOutput << "          json.chart = chart;\n";
    theOutput << "          json.title = title;\n";
    theOutput << "          json.tooltip = tooltip;\n";
    theOutput << "         json.series = series;\n";
    theOutput << "          json.plotOptions = plotOptions;\n";
    theOutput << "          $('#container').highcharts(json);\n";
    theOutput << "       });\n";
    theOutput << "    </script>\n";
    theOutput << " </body>\n";
    theOutput << "</html>\n";
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Format a Highcharts chart
 *
 * HTTP::Request parameter must contain 'highcharts'-parameter which
 * should contain name-value pairs separated with #-sign (%23).
 *
 * For example:
 * highcharts=charttype=linechart#xcolumn=1#ycolumns=2,3,4,#title=Temperature#subtitle=pal#groupbycolumn=2
 * or highcharts=charttype=piechart%23piecolumns=2,3,4,5,6,7,8,9,10%23title=Tuulen suunta
 * Uudellamaalla 28.10.2018 klo 19:00
 *
 */
// ----------------------------------------------------------------------

void HighChartsFormatter::format(std::ostream& theOutput,
                                 const Table& theTable,
                                 const TableFormatter::Names& theNames,
                                 const HTTP::Request& theReq,
                                 const TableFormatterOptions& /* theConfig */) const
{
  try
  {
    auto highcharts = theReq.getParameter("highcharts");
    if (!highcharts || highcharts->empty())
      throw Spine::Exception(BCP, "Missing or empty highcharts-parameter!");

    std::string highChartsString = *highcharts;

    std::vector<std::string> chartparams;
    boost::algorithm::split(chartparams, highChartsString, boost::algorithm::is_any_of("#"));

    HighChartsSettings hcSettings;
    for (const auto& p : chartparams)
    {
      if (p.empty())
        continue;
      std::vector<std::string> chartparam;
      boost::algorithm::split(chartparam, p, boost::algorithm::is_any_of("="));

      if (chartparam.size() != 2)
        throw Spine::Exception(BCP, "Invalid chart parameter format " + p + "!");

      hcSettings.insert(std::make_pair(chartparam[0], chartparam[1]));
    }

    std::string chartType;
    if (hcSettings.find(CHARTTYPE) != hcSettings.end())
      chartType = hcSettings.at(CHARTTYPE);

    if (chartType == LINECHART)
      linechart(theOutput, theTable, theNames, hcSettings);
    else if (chartType == PIECHART)
      piechart(theOutput, theTable, theNames, hcSettings);
    else
    {
      if (chartType.empty())
        throw Spine::Exception(BCP, "Missing charttype definition!");
      else
        throw Spine::Exception(BCP, "Unsupported chart type " + chartType + "!");
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
