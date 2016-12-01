#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>

#include <list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace SmartMet
{
namespace Spine
{
typedef boost::posix_time::ptime TimeStamp;
typedef std::vector<std::pair<std::string, std::string>> ParameterVector;
typedef std::vector<std::string> DetailVector;
typedef std::list<std::string> DetailList;

class Exception : public std::exception
{
 public:
  Exception();
  Exception(const Exception& _exception);

  // Use the following constructor when there is no previous exception in place

  Exception(const char* _filename, int _line, const char* _function, std::string _message);

  // Use the following constructor when there is previous exception in place
  // (i.e when you are in a "catch" block. If '_prevExeption' parameter is NULL then
  // the constructor automatically detects the content of the previous exception.

  Exception(const char* _filename,
            int _line,
            const char* _function,
            std::string _message,
            Exception* _prevException);

  // Destructor
  virtual ~Exception();

  // The following methods can be used for adding some additional information
  // related to the current exception.

  void addDetail(std::string _detailStr);
  void addDetails(const DetailList& _detailList);

  // This method can be used for adding named parameters into the exception.
  // The parameters can be "incorrect" values that caused the exception. They
  // can be also used for delivering additional information to the exception
  // catchers ("preferred HTTP status code",etc.).

  void addParameter(const char* _name, std::string _value);

  const char* what() const noexcept(true);
  const char* getWhat() const noexcept(true);

  std::string getFilename() const;
  int getLine() const;
  std::string getFunction() const;

  const Exception* getFirstException() const;
  const Exception* getExceptionByIndex(unsigned int _index) const;
  const Exception* getExceptionByParameterName(const char* _paramName) const;
  const Exception* getPrevException() const;

  unsigned int getExceptionCount() const;
  unsigned int getDetailCount() const;
  unsigned int getParameterCount() const;

  const char* getDetailByIndex(unsigned int _index) const;
  const char* getParameterNameByIndex(unsigned int _index) const;
  const char* getParameterValue(const char* _name) const;
  const char* getParameterValueByIndex(unsigned int _index) const;

  TimeStamp getTimeStamp() const;
  void setTimeStamp(TimeStamp _timestamp);

  std::string getStackTrace() const;
  std::string getHtmlStackTrace() const;

 protected:
  TimeStamp timestamp;
  std::string filename;
  int line;
  std::string function;
  std::string message;
  Exception* prevException;
  ParameterVector parameterVector;
  DetailVector detailVector;
};

#define BCP __FILE__, __LINE__, __PRETTY_FUNCTION__

}  // namespace Spine
}  // namespace SmartMet
