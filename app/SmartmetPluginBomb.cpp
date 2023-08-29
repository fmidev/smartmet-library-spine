#include "HTTP.h"
#include "Reactor.h"
#include <macgyver/Exception.h>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <exception>
#include <list>
#include <random>
#include <thread>
#include <future>

namespace ba = boost::algorithm;
namespace pt = boost::posix_time;
namespace http = SmartMet::Spine::HTTP;

using namespace SmartMet;

class BomberQuery
{
    const std::string uri;
    pt::ptime started;
    pt::ptime ended;
    std::size_t size;
    std::exception_ptr p_exception;

public:
    BomberQuery(const std::string& uri);
    BomberQuery(const BomberQuery&) = default;
    virtual ~BomberQuery() = default;
    BomberQuery& operator = (const BomberQuery&) = default;

    void run(Spine::Reactor* reactor);

    std::size_t get_size() const { return size; }
    std::exception_ptr get_exception() const { return p_exception; }
    void rethrow_exception() const { std::rethrow_exception(p_exception); }
    double elapsed() const;
    std::string get_uri() const { return uri; }

private:
    std::string make_query() const;
    void run_impl(Spine::Reactor* reactor, const std::string& query_str);
};

class BomberQueue
{
    std::mutex m;
    std::list<std::shared_ptr<BomberQuery> > data;
    std::size_t num_repetions;

public:
    BomberQueue(std::size_t num_repetions = 1);
    virtual ~BomberQueue() = default;

    void add(const std::string& uri);

    void shuffle();

    std::shared_ptr<BomberQuery> get();
};

class Bomber : public BomberQueue
{
    std::size_t num_threads;
    std::shared_ptr<Spine::Reactor> reactor;

public:
    Bomber(Spine::Options& options, std::size_t num_threads, std::size_t num_repetitions);
    virtual ~Bomber() = default;

    void run();

private:
    void bomber_thread_proc();
};

#define nm_help "help"
#define nm_config "config"
#define nm_input "input"

const char *sample =
    "192.168.14.17 - - [2023-08-29T03:31:18,897902]"
    " \"GET /timeseries?attributes=geoid&format=json&lang=en&latlon=63.8406915%2C23.1432863"
    "&param=geoid%2Cname%2Clatitude%2Clongitude%2Cregion%2Ccountry%2Ciso2%2Clocaltz&timesteps=1"
    "&who=MobileWeather HTTP/1.0\" 200 [2023-08-29T03:31:18,897571] 0 151 \"14399cf90b8cdb62-timeseries\" -";


boost::optional<std::string> parse_line(const std::string& line);

int main(int argc, char** argv)
{
    namespace po = boost::program_options;

    po::options_description desc("Allowed options");

    // clang-format off
    desc.add_options()
        (nm_help ",h", "Show this help message")
        (nm_config ",c", "Reactor config file")
        (nm_input ",i", "Input file with requests")
        ;

    po::variables_map opt;
    po::store(po::parse_command_line(argc, argv, desc), opt);

    po::notify(opt);

    if (opt.count(nm_help)) {
      std::cout << desc << std::endl;
      return 1;
    }

    SmartMet::Spine::Options options;
    std::string input_fn;

    if (opt.count(nm_config)) {
      options.configfile = opt[nm_config].as<std::string>();
    } else  {
      throw Fmi::Exception::Trace(BCP,
                                  "Mandatory command line option --config (or -c) missing");
    }
    options.port = 0;
    options.quiet = true;
    options.defaultlogging = false;
    options.accesslogdir = "/tmp";

    if (opt.count(nm_input)) {
        input_fn = opt[nm_input].as<std::string>();
    } else {
      throw Fmi::Exception::Trace(BCP,
          "Mandatory command line option --input (or -i) missing");
    }

    Bomber bomber(options, 10, 1);

    std::string line;
    std::ifstream input(input_fn.c_str());
    if (not input) {
      throw Fmi::Exception::Trace(BCP, "Failed to open " + input_fn);
    }

    while (std::getline(input, line)) {
        bomber.add(line);
    }

    bomber.run();

    return 0;
}

BomberQuery::BomberQuery(const std::string& uri)
    : uri(ba::trim_copy(uri))
    , size(0)
{
}

void
BomberQuery::run(Spine::Reactor* reactor)
{
    auto query_str = make_query();

    std::future<void> process =
        std::async(
            std::launch::async,
            [reactor, query_str, this]()
            {
                run_impl(reactor, query_str);
            });

    process.wait();
}

double BomberQuery::elapsed() const
{
    if (started.is_special() or ended.is_special()) {
        throw Fmi::Exception(BCP, "Timing not available");
    }

    return 0.000001 * (ended - started).total_microseconds();
}

std::string
BomberQuery::make_query() const
{
    std::ostringstream request;
    request << "GET " << uri << " HTTP/1.1\r\n";
    request << "\r\n";
    return request.str();
}

void
BomberQuery::run_impl(Spine::Reactor* reactor, const std::string& query_str)
{
    started = pt::microsec_clock::universal_time();
    auto parse_result = std::move(http::parseRequest(query_str));
    try {
        if (parse_result.first != http::ParsingStatus::COMPLETE) {
            throw Fmi::Exception(BCP, "Failed to parse query:" + ba::trim_copy(query_str));
        }

        auto view = reactor->getHandlerView(*parse_result.second);
        if (not view) {
            throw Fmi::Exception(BCP, "No handler for query:" + ba::trim_copy(query_str));
        }

        http::Response response;
        view->handle(*reactor, *parse_result.second, response);
        size = response.getContent().length();

        ended = pt::microsec_clock::universal_time();
    } catch (...) {
        p_exception = std::current_exception();
        ended = pt::microsec_clock::universal_time();
    }
}

BomberQueue::BomberQueue(std::size_t num_repetions)
    : num_repetions(num_repetions)
{
}

void BomberQueue::add(const std::string& uri)
{
    std::unique_lock<std::mutex> lock(m);
    for (std::size_t i = 0; i < num_repetions; i++) {
        data.push_back(std::make_shared<BomberQuery>(uri));
    }
}

void BomberQueue::shuffle()
{
    std::random_device rd;
    std::mt19937 g(rd());
    std::vector<std::shared_ptr<BomberQuery> > tmp;
    std::unique_lock<std::mutex> lock(m);
    std::move(data.begin(), data.end(), std::back_inserter(tmp));
    std::shuffle(tmp.begin(), tmp.end(), g);
    std::move(tmp.begin(), tmp.end(), std::back_inserter(data));
}

std::shared_ptr<BomberQuery> BomberQueue::get()
{
    std::unique_lock<std::mutex> lock(m);
    auto front = data.front();
    data.pop_front();
    return front;
}

Bomber::Bomber(
    Spine::Options& options,
    std::size_t num_threads,
    std::size_t num_repetitions)

    : BomberQueue(num_repetitions)
    , num_threads(num_threads)
{
    options.port = 0;

    reactor.reset(
        new Spine::Reactor(options),
        [](Spine::Reactor* reactor)
        {
            reactor->shutdown();
            delete reactor;
        });

    reactor->init();
    if (Spine::Reactor::isShuttingDown())
      throw Fmi::Exception(BCP, "Reactor shutdown detected while init phase");
}

void Bomber::run()
{
    std::vector<std::shared_ptr<std::thread> > bomber_threads;
    for (std::size_t i = 0; i < num_threads; i++) {
        bomber_threads.emplace_back(
            std::shared_ptr<std::thread>(
                new std::thread(
                    [this]()
                    {
                        bomber_thread_proc();
                    }),
                [](std::thread* t)
                {
                    if (t and t->joinable()) {
                        t->join();
                    }
                    delete t;
                } ));
    }
}

void Bomber::bomber_thread_proc()
{
    std::shared_ptr<BomberQuery> query;
    while ((query = get())) {
        std::ostringstream msg;
        query->run(reactor.get());
        msg << query->elapsed() << "  " << query->get_size() << "  " << query->get_uri() << '\n';
        std::cout << msg.str();
    }
}


