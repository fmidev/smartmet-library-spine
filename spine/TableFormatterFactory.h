// ======================================================================
/*!
 * \brief Interface of namespace TableFormatterFactory
 */
// ======================================================================

#pragma once

#include "TableFormatter.h"
#include <memory>
#include <string>

namespace SmartMet
{
namespace Spine
{
namespace TableFormatterFactory
{
SmartMet::Spine::TableFormatter* create(const std::string& theName);
}
}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
