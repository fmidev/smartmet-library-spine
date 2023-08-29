#include "HTTP.h"
#include "Reactor.h"
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <exception>
#include <thread>
#include <future>

using ba = boost::algorithm;
using pt = boost::posix_time;
using SmartMet::Spine::Reactor;

class BomberQuery
{
    const std::string uri;
    pt::ptime started;
    pt::ptime ended;
    std::size_t size;
    std::exception_ptr p_exception;

public:
    BomberQuery(const std::string& uri);
    virtual ~BomberQuery() = default;

    void run(Reactor* reactor);

private:
    std::string make_query() const;
    void run_impl(Reactor* reactor);
};

int main()
{
    return 0;
}


BomberQuery::BomberQuery(const std::string& uri)
    : uri(ba::trim_copy(uri))
    , size(0)
{
}

void BomberQuery::run(Reactor* reactor)
{
    
}

std::string BomberQuery::make_query() const
{
    std::ostringstream request;
    request << "GET " << uri << " HTTP/1.1\r\n";
    request << "\r\n";
    return request.str();
}
