// ======================================================================
/*!
 * \brief Convert an quality code from old code space to the new one.
 */
// ======================================================================

#pragma once

#include <map>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Spine
{
/**
 *  The class support full conversion from the old code space
 *  to the new code space.
 *
 *  A new code is one character of length and between [0,9].
 *  An old code is 1, 2, 3, or 4 character(s) of length and
 *  each of the character is a number between [0,9].
 *
 *  In a conversion only the first character (f) of an old
 *  code is important and the length of the code string
 *  (for example 'fxx' length is 3).
 *  So, if the length of the old code is 4 (fxxx) and the
 *  first character is 9 (F=f=9) the new code is 9, which
 *  is 'Erroneous value'.
 *
 * F      f   fx  fxx fxxx   Old code descr.   New code description
 * -   ---- ---- ---- ----   ----------------  -------------------
 * 0      0                  No checked        Unknown quality
 * 1      0    1    1    2   Checked and OK    Good quality
 * 2      3    3    3    4   Small difference  Accepted quality
 * 3      6    6    6    7   Big difference    Not accepted quality
 * 4      3    3    3    4   Calculated        Accepted quality
 * 5      3    3    3    4   Estimated         Accepted quality
 * 6      0    0    0    0   Not used          Unknown quality
 * 7      0    0    0    0   Not used          Unknown quality
 * 8      5    5    5    5   Missing           Missing value
 * 9      8    8    8    9   Deleted           Erroneous value
 *
 */
class QCConverter
{
 public:
  explicit QCConverter();
  virtual ~QCConverter() = default;
  QCConverter(const QCConverter& other) = delete;
  QCConverter& operator=(const QCConverter& other) = delete;

  /**
   *  \brief Convert an old code to a new one.
   *
   *  \param[out] newCode The new code that match with an old code.
   *  \param[in] oldCode An old code to convert to the new one.
   *  \return false if an conversion is not possible and otherwise true.
   */
  virtual bool convert(std::string& newCode, const std::string& oldCode);

 private:
  // Key of an elemet in the container express
  // the first character of an old key. The value
  // is the new key.
  using Entry = std::map<int, std::string>;
  // Position of an element in the container express
  // the length of an old key.
  std::vector<Entry> m_keys;
};

class QCConverterOpen : private QCConverter
{
 public:
  /**
   *  \brief If one character string, oldCode is the newCode.
   */
  bool convert(std::string& newCode, const std::string& oldCode) override;
};

}  // namespace Spine
}  // namespace SmartMet

// ======================================================================
