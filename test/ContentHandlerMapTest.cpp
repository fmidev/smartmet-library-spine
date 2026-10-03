#define BOOST_TEST_MODULE "ContentHandlerMapTest"

#include "ContentHandlerMap.h"
#include "HTTP.h"
#include "HandlerView.h"
#include "Options.h"
#include <boost/test/included/unit_test.hpp>

using namespace SmartMet::Spine;

namespace
{
void dummy(Reactor&, const HTTP::Request&, HTTP::Response&) {}

std::unique_ptr<ContentHandlerMap> make_map()
{
  Options options;
  options.quiet = true;
  auto map = std::make_unique<ContentHandlerMap>(options);
  map->addContentHandler(nullptr, "/timeseries", dummy);
  map->addContentHandler(nullptr, "/edr", dummy, {}, true);
  map->addContentHandler(nullptr, "/edr/collections", dummy, {}, true);
  map->addContentHandler(nullptr, "/wmts", dummy, {}, true);
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
