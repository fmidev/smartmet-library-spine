// ======================================================================
/*!
 * \brief Common definitions for threading data types
 */
// ----------------------------------------------------------------------

#pragma once

#include <mutex>
#include <shared_mutex>

namespace SmartMet
{
namespace Spine
{
// scoped read/write lock types

using MutexType = std::shared_mutex;
using ReadLock = std::shared_lock<MutexType>;
using WriteLock = std::unique_lock<MutexType>;

}  // namespace Spine
}  // namespace SmartMet
