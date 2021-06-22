#pragma once

#include <newbase/NFmiParameterName.h>

namespace SmartMet
{
namespace Spine
{
namespace Parameters
{
bool IsMetaParameter(FmiParameterName param);
bool IsLandscaped(FmiParameterName param);
bool IsDataIndependent(FmiParameterName param);
bool IsDataDerived(FmiParameterName param);
}  // namespace Parameters
}  // namespace Spine
}  // namespace SmartMet
