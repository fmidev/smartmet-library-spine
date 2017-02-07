// ======================================================================
/*!
 * \brief Common definitions for threading data types
 */
// ----------------------------------------------------------------------

#pragma once

#include <boost/thread.hpp>

namespace SmartMet
{
namespace Spine
{
// scoped read/write lock types

typedef boost::shared_mutex MutexType;
typedef boost::shared_lock<MutexType> ReadLock;
typedef boost::unique_lock<MutexType> WriteLock;
typedef boost::upgrade_lock<MutexType> UpgradeReadLock;
typedef boost::upgrade_to_unique_lock<MutexType> UpgradeWriteLock;

}  // namespace Spine
}  // namespace SmartMet
