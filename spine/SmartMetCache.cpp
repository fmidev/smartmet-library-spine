#include "SmartMetCache.h"
#include "Exception.h"
#include <vector>

#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

namespace SmartMet
{
namespace Spine
{
SmartMetCache::SmartMetCache(std::size_t memoryCacheSize,
                             std::size_t fileCacheSize,
                             const fs::path& cacheDirectory)
    : itsMemoryCache(memoryCacheSize),
      itsFileCache(cacheDirectory, fileCacheSize),
      itsShutdownRequested(false)
{
  try
  {
    itsFileThread.reset(new boost::thread(boost::bind(&SmartMetCache::operateFileCache, this)));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

SmartMetCache::~SmartMetCache()
{
  try
  {
    itsShutdownRequested = true;
    itsCondition.notify_one();
    boost::this_thread::sleep(boost::posix_time::milliseconds(200));
    itsFileThread->interrupt();
    itsFileThread->join();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

SmartMetCache::ValueType SmartMetCache::find(KeyType hash)
{
  try
  {
    // First search the in-memory cache
    auto memresult = itsMemoryCache.find(hash);

    if (memresult)
    {
      return *memresult;
    }

    // Next try the file cache
    auto fileresult = itsFileCache.find(hash);

    if (fileresult)
    {
      // Promote result to memcache and return
      auto entry = boost::make_shared<std::string>(std::move(*fileresult));

      std::vector<std::pair<KeyType, ValueType>> evictedItems;
      itsMemoryCache.insert(hash, entry, evictedItems);

      if (!evictedItems.empty())
      {
        queueFileWrites(evictedItems);
      }

      return entry;
    }
    else
    {
      return boost::shared_ptr<std::string>();
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void SmartMetCache::insert(KeyType hash, ValueType data)
{
  try
  {
    // Insert into in-memory cache
    std::vector<std::pair<KeyType, ValueType>> evictedItems;

    itsMemoryCache.insert(hash, data, evictedItems);

    if (!evictedItems.empty())
    {
      queueFileWrites(evictedItems);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::vector<std::pair<SmartMetCache::KeyType, SmartMetCache::ValueType>> SmartMetCache::getContent()
{
  try
  {
    std::vector<std::pair<KeyType, ValueType>> results;

    for (const auto& item : itsMemoryCache.getContent())
    {
      results.emplace_back(std::make_pair(item.itsKey, item.itsValue));
    }

    for (const auto& item : itsFileCache.getContent())
    {
      results.emplace_back(std::make_pair(item, nullptr));
    }

    return results;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void SmartMetCache::operateFileCache()
{
  try
  {
    while (!itsShutdownRequested)
    {
      boost::unique_lock<boost::mutex> theLock(itsMutex);
      if (itsPendingWrites.empty())
      {
        // Wait for something to write
        itsCondition.wait(theLock);
      }

      if (itsShutdownRequested)
        return;

      std::pair<KeyType, ValueType> back(
          itsPendingWrites.back());  // We need local copy here to increase locking granularity
      itsPendingWrites.pop_back();

      theLock.unlock();  // No longer operating on the stack.

      // This also performs filecache cleanup if needed
      itsFileCache.insert(back.first, *back.second);

      // Above might fail, but so what. Show must go on.
    }
  }
  catch (...)
  {
    SmartMet::Spine::Exception exception(BCP, "Operation failed!", NULL);
    std::cerr << exception.getStackTrace();
  }
}

void SmartMetCache::queueFileWrites(const std::vector<std::pair<KeyType, ValueType>>& items)
{
  try
  {
    boost::unique_lock<boost::mutex> theLock(itsMutex);
    for (auto& entry_pair : items)
    {
      // Insert this entry into the pending writes queue
      itsPendingWrites.emplace_front(entry_pair.first, entry_pair.second);
    }

    // Notify disk flusher
    itsCondition.notify_one();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void SmartMetCache::shutdown()
{
  itsShutdownRequested = true;
  itsCondition.notify_one();
}

}  // namespace Spine
}  // namespace SmartMet
