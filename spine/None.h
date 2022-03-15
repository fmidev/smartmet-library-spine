#pragma once

namespace SmartMet
{
namespace Spine
{

// Type to indicate empty ('nan') result
struct None
{
  // Nans are always identical to themselves, no nothing else
  template <class T>
  bool operator==(const T& other) const;
};

template <class T>
bool None::operator==(const T& /* other */) const
{
  return false;
}

template <>
bool None::operator==(const None& /* other */) const
{
  return true;
}

}  // namespace Spine
}  // namespace SmartMet
