#include "CRSRegistry.h"
#include <boost/thread.hpp>
#include <newbase/NFmiPoint.h>
#include <gdal_version.h>
#include <iostream>

using namespace SmartMet::Spine;

//
// Test for using same CRSRegistry::Transformation
// at the same time from several threads
//

namespace
{
struct TestEnv
{
  volatile bool done;
  boost::mutex mutex;
  boost::shared_ptr<CRSRegistry::Transformation> conv;
  boost::shared_ptr<CRSRegistry::Transformation> conv2;
  volatile long long num_run;
  volatile long long err_cnt;

  void add_cnt(int n1, int e1)
  {
    boost::mutex::scoped_lock lock(mutex);
    num_run += n1;
    err_cnt += e1;
  }
};

void test_thread_proc(TestEnv *env)
{
  while (not env->done)
  {
    int n1 = 0;
    int e1 = 0;
    for (int i = 0; i < 10000; i++)
    {
      NFmiPoint p1(24.0, 60.0), p2, p3;
      p2 = env->conv->transform(p1);
      n1++;
      if ((std::abs(332705.1789 - p2.X()) > 0.001) or (std::abs(6655205.4836 - p2.Y()) > 0.001))
      {
        e1++;
      }

      p3 = env->conv2->transform(p2);
      (void)p3;
    }
    env->add_cnt(n1, e1);
  }
}
}  // namespace

int main(void)
{
  const char *name = "CRSRegistry tester (parallel)";
  std::cout << "\n";
  std::cout << name << "\n";
  std::cout << std::string(std::strlen(name), '=') << std::endl;
  ;

  CRSRegistry registry;
  registry.register_epsg("WGS84", 4326);
  registry.register_epsg("UTM-ZONE-35", 4037);
  registry.register_epsg("WGS72-UTM-35", 32235);

  TestEnv env;
  env.done = false;
  env.conv = registry.create_transformation("WGS84", "UTM-ZONE-35");
  env.conv2 = registry.create_transformation("UTM-ZONE-35", "WGS84");
  env.num_run = 0;
  env.err_cnt = 0;

  const int N = 5;
  boost::thread_group test_threads;
  for (int i = 0; i < N; i++)
  {
    test_threads.create_thread(boost::bind(&test_thread_proc, &env));
  }

  sleep(3);
  env.done = true;

  test_threads.join_all();

  std::cout << "num_run=" << env.num_run << std::endl;
  std::cout << "num_err=" << env.err_cnt << std::endl;

  return env.err_cnt ? 1 : 0;
}
