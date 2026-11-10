#define BOOST_TEST_MODULE "ContentHandlerMapTest"

#include "ContentHandlerMap.h"
#include "HTTP.h"
#include "HandlerView.h"
#include "Options.h"
#include <boost/test/included/unit_test.hpp>

using namespace SmartMet::Spine;
using HandlerOptions = ContentHandlerMap::HandlerOptions;

namespace
{
void dummy(Reactor&, const HTTP::Request&, HTTP::Response&) {}

std::unique_ptr<ContentHandlerMap> make_map()
{
  Options options;
  options.quiet = true;
  auto map = std::make_unique<ContentHandlerMap>(options);
  map->addContentHandler(nullptr, HandlerOptions("/timeseries"), dummy);
  map->addContentHandler(nullptr, HandlerOptions("/edr").IsPrefix(true), dummy);
  map->addContentHandler(nullptr, HandlerOptions("/edr/collections").IsPrefix(true), dummy);
  map->addContentHandler(nullptr, HandlerOptions("/wmts").IsPrefix(true), dummy);
  return map;
}

// The URI of the handler selected for the resource, or "" for the no-match handler
std::string handler_for(ContentHandlerMap& map, const std::string& resource)
{
  HTTP::Request request;
  request.setResource(resource);
  auto* view = map.getHandlerView(request);
  if (view == nullptr || view->isCatchNoMatch())
    return "";
  return view->getResource();
}
}  // namespace

BOOST_AUTO_TEST_CASE(exact_handlers)
{
  auto map = make_map();
  BOOST_CHECK_EQUAL(handler_for(*map, "/timeseries"), "/timeseries");
  BOOST_CHECK_EQUAL(handler_for(*map, "/timeseries/foo"), "");
  BOOST_CHECK_EQUAL(handler_for(*map, "/timeseriesx"), "");
  BOOST_CHECK_EQUAL(handler_for(*map, "/nosuchhandler"), "");
}

BOOST_AUTO_TEST_CASE(prefix_handlers)
{
  auto map = make_map();
  BOOST_CHECK_EQUAL(handler_for(*map, "/wmts"), "/wmts");
  BOOST_CHECK_EQUAL(handler_for(*map, "/wmts/1.0.0/WMTSCapabilities.xml"), "/wmts");
  // The prefix must be followed by a path separator
  BOOST_CHECK_EQUAL(handler_for(*map, "/wmtsx"), "");
  BOOST_CHECK_EQUAL(handler_for(*map, "/edr/locations"), "/edr");
}

BOOST_AUTO_TEST_CASE(nested_prefixes)
{
  auto map = make_map();
  // The longest matching prefix should win
  BOOST_CHECK_EQUAL(handler_for(*map, "/edr/collections"), "/edr/collections");
  BOOST_CHECK_EQUAL(handler_for(*map, "/edr/collections/pal/position"), "/edr/collections");
}

BOOST_AUTO_TEST_CASE(has_handler_view)
{
  auto map = make_map();
  BOOST_CHECK(map->hasHandlerView("/timeseries"));
  BOOST_CHECK(map->hasHandlerView("/wmts/foo"));
  BOOST_CHECK(!map->hasHandlerView("/wmtsx"));
  BOOST_CHECK(!map->hasHandlerView("/nosuch"));
}

BOOST_AUTO_TEST_CASE(handler_options_1)
{
  std::vector<std::string> contentTypes = {"application/json", "application/xml"};
  ContentHandlerMap::HandlerOptions options("/test");
  options.Private(true);
  options.IsPrefix(true);
  BOOST_CHECK(options.isPrivate);
  BOOST_CHECK(options.handlesUriPrefix);
  options.addSupportedPostContentTypes(contentTypes.begin(), contentTypes.end());
  BOOST_CHECK(options.supportedPostContentTypes.count("application/json") == 1);
  BOOST_CHECK(options.supportedPostContentTypes.count("application/xml") == 1);
}

BOOST_AUTO_TEST_CASE(handler_options_2)
{
  ContentHandlerMap::HandlerOptions options("/test");
  options.addSupportedPostContentTypes("application/json", "application/xml", "text/plain");
  BOOST_CHECK(options.supportedPostContentTypes.count("application/json") == 1);
  BOOST_CHECK(options.supportedPostContentTypes.count("application/xml") == 1);
  BOOST_CHECK(options.supportedPostContentTypes.count("text/plain") == 1);
}
