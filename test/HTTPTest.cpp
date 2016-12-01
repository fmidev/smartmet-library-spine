#include "HTTP.h"
#include <string>
#include <iostream>

#include <boost/foreach.hpp>

#include <regression/tframe.h>

namespace HTTPTest
{
void noheaders()
{
  std::string request = "GET /test/server?param1=hei&param20=moi HTTP/1.0\r\n\r\n";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    if (req.second->toString() == request)
    {
      TEST_PASSED();
    }
    else
    {
      TEST_FAILED("Incorrect results on request: \n\"" + request + "\"\n\"" +
                  req.second->toString() + "\"");
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void spaces()
{
  std::string request = "GET /test/server?param1=hei+moi+space HTTP/1.0\r\n\r\n";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    if (req.second->toString() == "GET /test/server?param1=hei%20moi%20space HTTP/1.0\r\n\r\n")
    {
      TEST_PASSED();
    }
    else
    {
      TEST_FAILED("Incorrect results on request: \n\"" + request + "\"\n\"" +
                  req.second->toString() + "\"");
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void flagparam()
{
  std::string request = "GET /test/server?param1&param2=hei+moi&param3 HTTP/1.0\r\n\r\n";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    auto param1 = req.second->getParameter("param1");
    auto param3 = req.second->getParameter("param3");
    if (!param1)
    {
      TEST_FAILED("Parameter param1 not found");
    }
    if (!param3)
    {
      TEST_FAILED("Parameter param3 not found");
    }
    if (*param1 == "" && *param3 == "")
    {
      TEST_PASSED();
    }
    else
    {
      TEST_FAILED("Incorrect results on request: \n\"" + request + "\"\n\"" +
                  req.second->toString() + "\"");
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void emptyparampair()
{
  std::string request = "GET /test/server?&param1=hei&&param2=moi HTTP/1.0\r\n\r\n";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    if (req.second->toString() == "GET /test/server?param1=hei&param2=moi HTTP/1.0\r\n\r\n")
    {
      auto par1 = req.second->getParameter("param1");
      if (*par1 == "hei")
      {
        TEST_PASSED();
      }
      else
      {
        TEST_FAILED("Incorrect param1 value: " + *par1);
      }
    }
    else
    {
      TEST_FAILED("Incorrect results on request: \n\"" + request + "\"\n\"" +
                  req.second->toString() + "\"");
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }

  request = "GET /test/server?param1=hei&param2=moi&& HTTP/1.0\r\n\r\n";

  req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    if (req.second->toString() == "GET /test/server?param1=hei&param2=moi HTTP/1.0\r\n\r\n")
    {
      TEST_PASSED();
    }
    else
    {
      TEST_FAILED("Incorrect results on request: \n\"" + request + "\"\n\"" +
                  req.second->toString() + "\"");
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }

  request = "GET /test/server?param1=hei&&&&param2=moi&& HTTP/1.0\r\n\r\n";

  req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    if (req.second->toString() == "GET /test/server?param1=hei&param2=moi HTTP/1.0\r\n\r\n")
    {
      TEST_PASSED();
    }
    else
    {
      TEST_FAILED("Incorrect results on request: \n\"" + request + "\"\n\"" +
                  req.second->toString() + "\"");
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void emptyparam()
{
  std::string request = "GET /test/server?param1=&param2=2 HTTP/1.0\r\n\r\n";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    if (req.second->toString() == "GET /test/server?param1=&param2=2 HTTP/1.0\r\n\r\n")
    {
      TEST_PASSED();
    }
    else
    {
      TEST_FAILED("Incorrect results on request: \n\"" + request + "\"\n\"" +
                  req.second->toString() + "\"");
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void emptylastparam()
{
  std::string request = "GET /test/server?param1= HTTP/1.0\r\n\r\n";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    if (req.second->toString() == "GET /test/server?param1= HTTP/1.0\r\n\r\n")
    {
      TEST_PASSED();
    }
    else
    {
      TEST_FAILED("Incorrect results on request: \n\"" + request + "\"\n\"" +
                  req.second->toString() + "\"");
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void testplus()
{
  std::string request = "GET /test/server?param1=moi+%2B+moi HTTP/1.0\r\n\r\n";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    auto res = req.second->getParameter("param1");
    if (*res == "moi + moi")
    {
      TEST_PASSED();
    }
    else
    {
      TEST_FAILED("Incorrect results on request: \n\"" + request + "\"\n\"" + *res + "\"");
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void obfuscated_header()
{
  // Here header-like entity is actually part of the body
  std::string request = "GET /test/server?param1=hei+moi+space HTTP/1.0\r\n\r\n";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    if (req.second->toString() == "GET /test/server?param1=hei%20moi%20space HTTP/1.0\r\n\r\n")
    {
      TEST_PASSED();
    }
    else
    {
      TEST_FAILED("Incorrect results on request: \n\"" + request + "\"\n\"" +
                  req.second->toString() + "\"");
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void qquery()
{
  std::string request =
      "GET "
      "/test/"
      "server?paramId=1&validTime=20130307120000&producerId=230&dataType=2&projection="
      "stereographic,20.0,90.0,60.0:-5.7643213,48.5511121,67.0738805,64.7340172&gridSize=40,40&"
      "maxDecimals=1&requestType=grid&format=png&contour=0%200%20966%20553&c1=1%205.0%200.0%20900."
      "0%201050.0%20rgba(40,40,40,160)%20none%201.0%20%202.0%203%2016%20def%20none%20none%201 "
      "HTTP/1.0\r\n\r\n";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    auto param = req.second->getParameter("c1");
    if (!param)
    {
      TEST_FAILED("Parameter \"c1\" not found.");
    }
    if (*param != "1 5.0 0.0 900.0 1050.0 rgba(40,40,40,160) none 1.0  2.0 3 16 def none none 1")
    {
      TEST_FAILED("Incorrect results on parameter Name: \"" + *param + "\"");
    }

    TEST_PASSED();
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void qquerylua()
{
  std::string request =
      "GET "
      "/test/"
      "server?code=local+locs%3D{latlon%2860.89%2C26.94%29}+local+r%2Cerr%3D+HIR{hybrid%3Dtrue}"
      "assert%28r%2Cerr%29+return+cross%28+r%2C+P%2C+locs%2C+validtime%29%2Ccross%28+r%2C+%22%3A3%"
      "22%2C+locs%2C+validtime+%29&validtime=20130305090000&decimals=1 HTTP/1.0\r\n\r\n";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    std::string result = req.second->toString();

    auto parsedBack = SmartMet::Spine::HTTP::parseRequest(result);

    auto param = parsedBack.second->getParameter("code");
    if (!param)
    {
      TEST_FAILED("Parameter \"code\" not found.");
    }
    if (*param !=
        "local locs={latlon(60.89,26.94)} local r,err= HIR{hybrid=true}assert(r,err) return cross( "
        "r, P, locs, validtime),cross( r, \":3\", locs, validtime )")
    {
      TEST_FAILED("Incorrect results on parameter Name: \"" + *param + "\"");
    }

    TEST_PASSED();
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void encodequery()
{
  std::string request =
      "GET "
      "/test/"
      "server?code=local%20locs%3D%7Blatlon%2860.89%2C26.94%29%7D%20local%20r%2Cerr%3D%20HIR%"
      "7Bhybrid%3Dtrue%7Dassert%28r%2Cerr%29%20return%20cross%28%20r%2C%20P%2C%20locs%2C%"
      "20validtime%29%2Ccross%28%20r%2C%20%22%3A3%22%2C%20locs%2C%20validtime%20%29 "
      "HTTP/1.0\r\n\r\n";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    if (req.second->toString() == request)
    {
      TEST_PASSED();
    }
    else
    {
      TEST_FAILED("Incorrect results on request: \n\"" + request + "\"\n\"" +
                  req.second->toString() + "\"");
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void multipleget()
{
  std::string request = "GET /test/server?param1=hei&param1=moi HTTP/1.0\r\n\r\n";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    if (req.second->toString() == request)
    {
      TEST_PASSED();
    }
    else
    {
      TEST_FAILED("Incorrect results on request: \n\"" + request + "\"\n\"" +
                  req.second->toString() + "\"");
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void withheaders()
{
  std::string request =
      "GET /test/server?param1=hei&param20=moi HTTP/1.0\r\nFrom: tuomo.lauri@fmi.fi\r\nUser-Agent: "
      "FakeBrowser\r\n\r\n";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    if (req.second->toString() == request)
    {
      TEST_PASSED();
    }
    else
    {
      TEST_FAILED("Incorrect results on request: \n\"" + request + "\"\n\"" +
                  req.second->toString() + "\"");
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void headersandcontent()
{
  std::string request =
      "POST /test/server HTTP/1.0\r\nContent-Length: 11\r\nContent-Type: text/ascii\r\nFrom: "
      "tuomo.lauri@fmi.fi\r\nUser-Agent: FakeBrowser\r\n\r\nBodyContent";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    if (req.second->toString() == request)
    {
      TEST_PASSED();
    }
    else
    {
      TEST_FAILED("Incorrect results on request: \n\"" + request + "\"\n\"" +
                  req.second->toString() + "\"");
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void post()
{
  std::string request =
      "POST /test/server HTTP/1.0\r\nContent-Length: 11\r\nContent-Type: text/ascii\r\nFrom: "
      "tuomo.lauri@fmi.fi\r\nUser-Agent: FakeBrowser\r\n\r\nBodyContent";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    if (req.second->toString() == request)
    {
      TEST_PASSED();
    }
    else
    {
      TEST_FAILED("Incorrect results on request: \n\"" + request + "\"\n\"" +
                  req.second->toString() + "\"");
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void postwitharguments()
{
  std::string request =
      "POST /test/server?foo1=bar&foor2=baz HTTP/1.0\r\nContent-Length: 11\r\nContent-Type: "
      "text/ascii\r\nFrom: tuomo.lauri@fmi.fi\r\nUser-Agent: FakeBrowser\r\n\r\nBodyContent";

  std::string correct =
      "POST /test/server HTTP/1.0\r\nContent-Length: 11\r\nContent-Type: text/ascii\r\nFrom: "
      "tuomo.lauri@fmi.fi\r\nUser-Agent: FakeBrowser\r\n\r\nBodyContent";  // Get-like parameters
                                                                           // are ignored since they
                                                                           // can conflict with body
                                                                           // content

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    if (req.second->toString() == correct)
    {
      TEST_PASSED();
    }
    else
    {
      TEST_FAILED("Incorrect results on request: \n\"" + request + "\"\n\"" +
                  req.second->toString() + "\"");
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void urlencoded()
{
  std::string request =
      "POST /test/server HTTP/1.0\r\nContent-Length: 52\r\nContent-Type: "
      "application/x-www-form-urlencoded\r\nFrom: tuomo.lauri@fmi.fi\r\nUser-Agent: "
      "FakeBrowser\r\n\r\nName=John+Doe&Age=28&Formula=a+%2B+b+%3D%3D+13%25%21";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    auto param = req.second->getParameter("Name");
    if (!param)
    {
      TEST_FAILED("POST parameter \"Name\" not found.");
    }
    if (*param != "John Doe")
    {
      TEST_FAILED("Incorrect results on parameter Name: \"" + *param + "\"");
    }

    param = req.second->getParameter("Age");
    if (!param)
    {
      TEST_FAILED("POST parameter \"Age\" not found.");
    }
    if (*param != "28")
    {
      TEST_FAILED("Incorrect results on parameter Age: \"" + *param + "\"");
    }

    param = req.second->getParameter("Formula");
    if (!param)
    {
      TEST_FAILED("POST parameter \"Formula\" not found.");
    }
    if (*param != "a + b == 13%!")
    {
      TEST_FAILED("Incorrect results on parameter Formula: \"" + *param + "\"");
    }

    TEST_PASSED();
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void postandget()
{
  std::string request =
      "POST /test/server?param1=foo%20bar HTTP/1.0\r\nContent-Length: 52\r\nContent-Type: "
      "application/x-www-form-urlencoded\r\nFrom: tuomo.lauri@fmi.fi\r\nUser-Agent: "
      "FakeBrowser\r\n\r\nName=John+Doe&Age=28&Formula=a+%2B+b+%3D%3D+13%25%21";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    auto param = req.second->getParameter("Name");
    if (!param)
    {
      TEST_FAILED("POST parameter \"Name\" not found.");
    }
    if (*param != "John Doe")
    {
      TEST_FAILED("Incorrect results on parameter Name: \"" + *param + "\"");
    }

    param = req.second->getParameter("Age");
    if (!param)
    {
      TEST_FAILED("POST parameter \"Age\" not found.");
    }
    if (*param != "28")
    {
      TEST_FAILED("Incorrect results on parameter Age: \"" + *param + "\"");
    }

    param = req.second->getParameter("Formula");
    if (!param)
    {
      TEST_FAILED("POST parameter \"Formula\" not found.");
    }
    if (*param != "a + b == 13%!")
    {
      TEST_FAILED("Incorrect results on parameter Formula: \"" + *param + "\"");
    }

    param = req.second->getParameter("param1");
    if (!param)
    {
      TEST_FAILED("GET parameter \"param1\" not found.");
    }
    if (*param != "foo bar")
    {
      TEST_FAILED("Incorrect results on parameter param1: \"" + *param + "\"");
    }

    TEST_PASSED();
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void caseinsensitiveurlencoded()
{
  std::string request =
      "POST /test/server HTTP/1.0\r\nContent-Length: 52\r\nContent-Type: "
      "application/x-www-form-urlencoded\r\nFrom: tuomo.lauri@fmi.fi\r\nUser-Agent: "
      "FakeBrowser\r\n\r\nNAMe=John+Doe&AGE=28&ForMUlA=a+%2B+b+%3D%3D+13%25%21";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    auto param = req.second->getParameter("Name");
    if (!param)
    {
      TEST_FAILED("POST parameter \"Name\" not found.");
    }
    if (*param != "John Doe")
    {
      TEST_FAILED("Incorrect results on parameter Name: \"" + *param + "\"");
    }

    param = req.second->getParameter("Age");
    if (!param)
    {
      TEST_FAILED("POST parameter \"Age\" not found.");
    }
    if (*param != "28")
    {
      TEST_FAILED("Incorrect results on parameter Age: \"" + *param + "\"");
    }

    param = req.second->getParameter("Formula");
    if (!param)
    {
      TEST_FAILED("POST parameter \"Formula\" not found.");
    }
    if (*param != "a + b == 13%!")
    {
      TEST_FAILED("Incorrect results on parameter Formula: \"" + *param + "\"");
    }

    TEST_PASSED();
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void caseinsensitiveheaders()
{
  std::string request =
      "GET /test/server?param1=hei&param20=moi HTTP/1.0\r\nConTent-LeNGth: 11\r\nContent-TYPE: "
      "text/ascii\r\nFrom: tuomo.lauri@fmi.fi\r\nUser-Agent: FakeBrowser\r\n\r\nBodyContent";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    auto param = req.second->getHeader("content-length");
    if (!param)
    {
      TEST_FAILED("Header \"Content-length\" not found.");
    }

    param = req.second->getHeader("content-type");
    if (!param)
    {
      TEST_FAILED("Header \"content-type\" not found.");
    }

    TEST_PASSED();
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void complexurlencoded()
{
  std::string request =
      "POST /test/server HTTP/1.0\r\nContent-Length: 63\r\nContent-Type: "
      "application/x-www-form-urlencoded\r\nFrom: tuomo.lauri@fmi.fi\r\nUser-Agent: "
      "FakeBrowser\r\n\r\nName=John+Doe&Age=28(%a%20%b)&Formula=a+%2B+b+%3D%3D+13%25%21%G";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    auto param = req.second->getParameter("Name");
    if (!param)
    {
      TEST_FAILED("POST parameter \"Name\" not found.");
    }
    if (*param != "John Doe")
    {
      TEST_FAILED("Incorrect results on parameter Name: \"" + *param + "\"");
    }

    param = req.second->getParameter("Age");
    if (!param)
    {
      TEST_FAILED("POST parameter \"Age\" not found.");
    }
    if (*param != "28(%a %b)")
    {
      TEST_FAILED("Incorrect results on parameter Age: \"" + *param + "\"");
    }

    param = req.second->getParameter("Formula");
    if (!param)
    {
      TEST_FAILED("POST parameter \"Formula\" not found.");
    }
    if (*param != "a + b == 13%!%G")
    {
      TEST_FAILED("Incorrect results on parameter Formula: \"" + *param + "\"");
    }

    TEST_PASSED();
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void emptyrequest()
{
  std::string request = "GET /serveri HTTP/1.0\r\n\r\n";

  auto req = SmartMet::Spine::HTTP::parseRequest(request);

  if (req.first == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    std::string res = req.second->getResource();
    if (res == "/serveri")
    {
      TEST_PASSED();
    }
    else
    {
      TEST_FAILED("Wrong resource in parsed request: " + res);
    }
  }
  else
  {
    TEST_FAILED("Parse failed on request: " + request);
  }
}

void responseparse()
{
  std::string raw_response =
      "HTTP/1.0 503 Service Unavailable\r\nContent-Length: 9\r\n\r\nMoikkamoi";

  auto req = SmartMet::Spine::HTTP::parseResponse(raw_response);

  if (std::get<0>(req) == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    auto& response = std::get<1>(req);

    auto param = response->getHeader("Content-Length");
    if (!param)
    {
      TEST_FAILED("Header \"Content-Length\" not found.");
    }
    if (*param != "9")
    {
      TEST_FAILED("Incorrect results on parameter Name: \"" + *param + "\"");
    }

    std::string content = std::string(std::get<2>(req), raw_response.cend());

    if (content != "Moikkamoi")
    {
      TEST_FAILED("Incorrect response content");
    }

    if (response->getReasonPhrase() != "Service Unavailable")
    {
      TEST_FAILED("Incorrect response reason phrase");
    }

    TEST_PASSED();
  }
  else
  {
    TEST_FAILED("Parse failed on response: " + raw_response);
  }
}

void hexresponseparse()
{
  std::string raw_response =
      "\x48\x54\x54\x50\x2f\x31\x2e\x30\x20\x32\x30\x30\x20\x4f\x4b\x0d\x0a\x43\x61\x63\x68\x65\x2d"
      "\x43\x6f\x6e\x74\x72\x6f\x6c\x3a\x20\x70\x75\x62\x6c\x69\x63\x2c\x20\x6d\x61\x78\x2d\x61\x67"
      "\x65\x3d\x36\x30\x0d\x0a\x43\x6f\x6e\x74\x65\x6e\x74\x2d\x4c\x65\x6e\x67\x74\x68\x3a\x20\x31"
      "\x33\x37\x32\x33\x37\x0d\x0a\x43\x6f\x6e\x74\x65\x6e\x74\x2d\x54\x79\x70\x65\x3a\x20\x69\x6d"
      "\x61\x67\x65\x2f\x70\x6e\x67\x0d\x0a\x44\x61\x74\x65\x3a\x20\x54\x75\x65\x2c\x20\x32\x34\x20"
      "\x4d\x61\x72\x20\x32\x30\x31\x35\x20\x31\x31\x3a\x32\x30\x3a\x31\x30\x20\x47\x4d\x54\x0d\x0a"
      "\x45\x78\x70\x69\x72\x65\x73\x3a\x20\x54\x75\x65\x2c\x20\x32\x34\x20\x4d\x61\x72\x20\x32\x30"
      "\x31\x35\x20\x31\x31\x3a\x32\x31\x3a\x31\x30\x20\x47\x4d\x54\x0d\x0a\x4c\x61\x73\x74\x2d\x4d"
      "\x6f\x64\x69\x66\x69\x65\x64\x3a\x20\x54\x75\x65\x2c\x20\x32\x34\x20\x4d\x61\x72\x20\x32\x30"
      "\x31\x35\x20\x31\x31\x3a\x32\x30\x3a\x31\x30\x20\x47\x4d\x54\x0d\x0a\x53\x65\x72\x76\x65\x72"
      "\x3a\x20\x42\x72\x61\x69\x6e\x53\x74\x6f\x72\x6d\x20\x53\x79\x6e\x61\x70\x73\x65\x20\x28\x30"
      "\x38\x3a\x31\x31\x3a\x31\x39\x20\x4d\x61\x72\x20\x31\x30\x20\x32\x30\x31\x35\x29\x0d\x0a\x56"
      "\x61\x72\x79\x3a\x20\x41\x63\x63\x65\x70\x74\x2d\x45\x6e\x63\x6f\x64\x69\x6e\x67\x0d\x0a\x0d"
      "\x0a\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52\x00\x00\x02\x58\x00\x00"
      "\x02\x58\x08\x02\x00\x00\x00\x31\x04\x0f\x8b\x00\x00\x00\x06\x62\x4b\x47\x44\x00\xff\x00\xff"
      "\x00\xff\xa0";

  auto req = SmartMet::Spine::HTTP::parseResponse(raw_response);

  if (std::get<0>(req) == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    auto& response = std::get<1>(req);

    auto param = response->getHeader("Content-Length");
    if (!param)
    {
      TEST_FAILED("Header \"Content-Length\" not found.");
    }
    if (*param != "137237")
    {
      TEST_FAILED("Incorrect results on parameter Name: \"" + *param + "\"");
    }

    if (response->getReasonPhrase() != "OK")
    {
      TEST_FAILED("Incorrect response reason phrase");
    }

    TEST_PASSED();
  }
  else
  {
    TEST_FAILED("Parse failed on response: " + raw_response);
  }
}

void extendedresponseparse()
{
  std::string raw_response =
      "HTTP/1.0 200 OK\r\nCache-Control: public, max-age=60\r\nContent-Encoding: "
      "gzip\r\nContent-Length: 999\r\nContent-Type: image/png\r\nDate: Fri, 20 Mar 2015 08:42:28 "
      "GMT\r\nETag: \"e2fc17fb9c20f2e2\"\r\nExpires: Fri, 20 Mar 2015 08:43:28 "
      "GMT\r\nLast-Modified: Fri, 20 Mar 2015 08:42:28 GMT\r\nServer: Smartmet (08:11:19 "
      "Mar 10 2015)\r\nVary: Accept-Encoding\r\n\r\n";

  auto req = SmartMet::Spine::HTTP::parseResponse(raw_response);

  if (std::get<0>(req) == SmartMet::Spine::HTTP::ParsingStatus::COMPLETE)
  {
    auto& response = std::get<1>(req);

    auto param = response->getHeader("Content-Length");
    if (!param)
    {
      TEST_FAILED("Header \"Content-Length\" not found.");
    }
    if (*param != "999")
    {
      TEST_FAILED("Incorrect results on parameter Name: \"" + *param + "\"");
    }

    if (response->getReasonPhrase() != "OK")
    {
      TEST_FAILED("Incorrect response reason phrase");
    }

    TEST_PASSED();
  }
  else
  {
    TEST_FAILED("Parse failed on response: " + raw_response);
  }
}

void caseinsensitivecomp()
{
  SmartMet::Spine::HTTP::CaseInsensitiveComp cmp;

  if (cmp("a", "") != false)
    TEST_FAILED("a > '' failed");
  if (cmp("", "b") != true)
    TEST_FAILED("'' < b failed");
  if (cmp("", "") != false)
    TEST_FAILED("'' == '' failed");

  if (cmp("a", "a") != false)
    TEST_FAILED("a == a failed");
  if (cmp("a", "b") != true)
    TEST_FAILED("a < b failed");
  if (cmp("b", "a") != false)
    TEST_FAILED("b > a failed");

  if (cmp("abba", "abba") != false)
    TEST_FAILED("abba == abba failed");
  if (cmp("abba1", "abba") != false)
    TEST_FAILED("abba1 > abba failed");
  if (cmp("abba", "abba2") != true)
    TEST_FAILED("abba < abba2 failed");
  if (cmp("abba1", "abba2") != true)
    TEST_FAILED("abba1 < abba2 failed");

  if (cmp("a", "A") != false)
    TEST_FAILED("a == a failed");
  if (cmp("a", "B") != true)
    TEST_FAILED("a < b failed");
  if (cmp("B", "a") != false)
    TEST_FAILED("b > a failed");

  if (cmp("A", "A") != false)
    TEST_FAILED("a == a failed");
  if (cmp("A", "b") != true)
    TEST_FAILED("a < b failed");
  if (cmp("b", "A") != false)
    TEST_FAILED("b > a failed");

  if (cmp("abba", "ABba") != false)
    TEST_FAILED("abba == abba failed");
  if (cmp("abba1", "aBBa") != false)
    TEST_FAILED("abba1 > abba failed");
  if (cmp("abbA", "abBa2") != true)
    TEST_FAILED("abba < abba2 failed");
  if (cmp("AbBa1", "abBA2") != true)
    TEST_FAILED("abba1 < abba2 failed");

  TEST_PASSED();
}

class tests : public tframe::tests
{
  virtual const char* error_message_prefix() const { return "\n\t"; }
  void test(void)
  {
    TEST(caseinsensitivecomp);
    TEST(noheaders);
    TEST(spaces);
    TEST(testplus);
    TEST(emptyparampair);
    TEST(emptyparam);
    TEST(emptyrequest);
    TEST(emptylastparam);
    TEST(obfuscated_header);
    TEST(qquery);
    TEST(qquerylua);
    TEST(encodequery);
    TEST(withheaders);
    TEST(headersandcontent);
    TEST(post);
    TEST(postwitharguments);
    TEST(urlencoded);
    TEST(multipleget);
    TEST(complexurlencoded);
    TEST(responseparse);
    TEST(hexresponseparse);
    TEST(extendedresponseparse);
    TEST(caseinsensitiveheaders);
    TEST(caseinsensitiveurlencoded);
    TEST(postandget);
    TEST(flagparam);
  }
};
}

int main()
{
  std::cout << std::endl << "HTTP tester" << std::endl << "========================" << std::endl;
  HTTPTest::tests t;
  return t.run();
}
