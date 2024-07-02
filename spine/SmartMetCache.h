#pragma once
#include <boost/filesystem.hpp>
#include <memory>
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
  static std::size_t getSize(const std::shared_ptr<std::string>& theValue)
  {
    return theValue->size();
  }
};

/*
      ----------------------------------------
      * Memory and filesystem - backed cache
      * for plugin and engine use
      * ----------------------------------------
      */
class SmartMetCache
{
  using KeyType = std::size_t;

  using ValueType = std::shared_ptr<std::string>;

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

  SmartMetCache(const SmartMetCache& other) = delete;
  SmartMetCache(SmartMetCache&& other) = delete;
  SmartMetCache& operator=(const SmartMetCache& other) = delete;
  SmartMetCache& operator=(SmartMetCache&& other) = delete;

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
  void insert(KeyType hash, const ValueType& data);

  /*
   *----------------------------------------
   * Get cache contents
   * For filesystem entries, the pointer value is
   * nullptr
   * ----------------------------------------
   */

  std::vector<std::pair<KeyType, ValueType>> getContent();

  void shutdown();

  Fmi::Cache::CacheStats getMemoryCacheStats() const { return itsMemoryCache.statistics(); }
  Fmi::Cache::CacheStats getFileCacheStats() const
  {
    return (itsFileCache ? itsFileCache->statistics() : Fmi::Cache::EMPTY_CACHE_STATS);
  }

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
