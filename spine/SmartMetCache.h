#pragma once
#include <boost/filesystem.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include <deque>
#include <string>

#include <macgyver/Cache.h>

namespace SmartMet
{
namespace Spine
{
namespace fs = boost::filesystem;

struct BufferSizeFunction
{
  static std::size_t getSize(boost::shared_ptr<std::string> theValue) { return theValue->size(); }
};

/*
      ----------------------------------------
      * Memory and filesystem - backed cache
      * for plugin and engine use
      * ----------------------------------------
      */
class SmartMetCache : boost::noncopyable
{
  typedef std::size_t KeyType;

  typedef boost::shared_ptr<std::string> ValueType;

 public:
  /*
    ----------------------------------------
    * Constructor
    * - memoryCacheSize is memory cache size in entries
    * - fileCacheSize is file cache size in bytes
    * - cacheDirectory is cache directory in filesystem
    * ----------------------------------------
    */
  SmartMetCache(std::size_t memoryCacheSize,
                std::size_t fileCacheSize,
                const fs::path& cacheDirectory);

  ~SmartMetCache();

  /*
   * ----------------------------------------
   * Find key in cache
   * ----------------------------------------
   */
  ValueType find(KeyType hash);

  /*
   *----------------------------------------
   * Insert new entry into the cache
   * ----------------------------------------
   */
  void insert(KeyType hash, ValueType data);

  /*
   *----------------------------------------
   * Get cache contents
   * For filesystem entries, the pointer value is
   * nullptr
   * ----------------------------------------
   */

  std::vector<std::pair<KeyType, ValueType>> getContent();

  void shutdown();

 private:
  void operateFileCache();

  void queueFileWrites(const std::vector<std::pair<KeyType, ValueType>>& items);

  Fmi::Cache::Cache<KeyType,
                    ValueType,
                    Fmi::Cache::LRUEviction,
                    int,
                    Fmi::Cache::StaticExpire,
                    BufferSizeFunction>
      itsMemoryCache;

  std::unique_ptr<Fmi::Cache::FileCache> itsFileCache;

  std::deque<std::pair<KeyType, ValueType>> itsPendingWrites;

  std::unique_ptr<boost::thread> itsFileThread;

  // This guards the itsPendingWrites stack
  boost::mutex itsMutex;  // For condition variable, shared mutex won't do.

  boost::condition_variable itsCondition;
  boost::atomic<bool> itsShutdownRequested{false};
};

}  // namespace Spine
}  // namespace SmartMet
