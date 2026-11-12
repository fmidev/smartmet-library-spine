// ======================================================================
/*!
 * \brief Interface of namespace TableFormatterFactory
 */
// ======================================================================

#pragma once

#include <string>

namespace SmartMet::Spine
{
class TableFormatter;
}  // namespace SmartMet::Spine

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
