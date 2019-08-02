#include "Exception.h"
#include "Convenience.h"
#include <macgyver/AnsiEscapeCodes.h>
#include <iostream>
#include <sstream>

namespace SmartMet
{
namespace Spine
{
Exception::Exception()
{
  timestamp = boost::posix_time::second_clock::local_time();
  line = 0;
  prevException = nullptr;
}

Exception::Exception(const Exception& other)
{
  timestamp = other.timestamp;
  filename = other.filename;
  line = other.line;
  function = other.function;
  message = other.message;
  detailVector = other.detailVector;
  parameterVector = other.parameterVector;
  prevException = nullptr;
  mStackTraceDisabled = other.mStackTraceDisabled;
  mLoggingDisabled = other.mLoggingDisabled;

  if (other.prevException != nullptr)
    prevException = new Exception(*other.prevException);
}

Exception Exception::Trace(const char* _filename,
                           int _line,
                           const char* _function,
                           std::string _message)
{
  return Exception(_filename, _line, _function, std::move(_message), nullptr);
}

Exception::Exception(const char* _filename,
                     int _line,
                     const char* _function,
                     std::string _message,
                     Exception* _prevException)
{
  timestamp = boost::posix_time::second_clock::local_time();
  filename = _filename;
  line = _line;
  function = _function;
  message = std::move(_message);
  prevException = nullptr;
  if (_prevException != nullptr)
  {
    prevException = new Exception(*_prevException);
  }
  else if (std::current_exception())
  {
    try
    {
      throw;
    }
    catch (SmartMet::Spine::Exception& e)
    {
      prevException = new Exception(e);
      // Propagate the flags to the top
      mStackTraceDisabled = e.mStackTraceDisabled;
      mLoggingDisabled = e.mLoggingDisabled;
    }
    catch (std::out_of_range& e)
    {
      prevException =
          new Exception(_filename, _line, _function, std::string("[Out of range] ") + e.what());
    }
    catch (std::invalid_argument& e)
    {
      prevException =
          new Exception(_filename, _line, _function, std::string("[Invalid argument] ") + e.what());
    }
    catch (std::length_error& e)
    {
      prevException =
          new Exception(_filename, _line, _function, std::string("[Length error] ") + e.what());
    }
    catch (std::domain_error& e)
    {
      prevException =
          new Exception(_filename, _line, _function, std::string("[Domain error] ") + e.what());
    }
    catch (std::range_error& e)
    {
      prevException =
          new Exception(_filename, _line, _function, std::string("[Range error] ") + e.what());
    }
    catch (std::overflow_error& e)
    {
      prevException =
          new Exception(_filename, _line, _function, std::string("[Overflow error] ") + e.what());
    }
    catch (std::underflow_error& e)
    {
      prevException =
          new Exception(_filename, _line, _function, std::string("[Underflow error] ") + e.what());
    }
    catch (std::runtime_error& e)
    {
      prevException =
          new Exception(_filename, _line, _function, std::string("[Runtime error] ") + e.what());
    }
    catch (std::logic_error& e)
    {
      prevException =
          new Exception(_filename, _line, _function, std::string("[Logic error] ") + e.what());
    }
    catch (std::exception& e)
    {
      prevException =
          new Exception(_filename, _line, _function, std::string("[Exception] ") + e.what());
    }
    catch (...)
    {
      prevException = new Exception(_filename, _line, _function, "Unknown exception");
    }
  }
}

Exception::Exception(const char* _filename, int _line, const char* _function, std::string _message)
{
  timestamp = boost::posix_time::second_clock::local_time();
  filename = _filename;
  line = _line;
  function = _function;
  message = std::move(_message);
  prevException = nullptr;
}

Exception::~Exception()
{
  delete prevException;
}

std::string Exception::getFilename() const
{
  return filename;
}

int Exception::getLine() const
{
  return line;
}

std::string Exception::getFunction() const
{
  return function;
}

const char* Exception::getWhat() const noexcept(true)
{
  return message.c_str();
}

const char* Exception::what() const noexcept(true)
{
  return getFirstException()->getWhat();
}

const Exception* Exception::getPrevException() const
{
  return prevException;
}

const Exception* Exception::getFirstException() const
{
  if (prevException != nullptr)
    return prevException->getFirstException();

  return this;
}

TimeStamp Exception::getTimeStamp() const
{
  return timestamp;
}

unsigned int Exception::getExceptionCount() const
{
  unsigned int count = 0;
  const Exception* e = this;
  while (e != nullptr)
  {
    count++;
    e = e->getPrevException();
  }
  return count;
}

const Exception* Exception::getExceptionByIndex(unsigned int _index) const
{
  unsigned int count = 0;
  const Exception* e = this;
  while (e != nullptr)
  {
    if (count == _index)
      return e;

    count++;
    e = e->getPrevException();
  }
  return nullptr;
}

void Exception::setTimeStamp(TimeStamp _timestamp)
{
  timestamp = _timestamp;
}

Exception& Exception::addDetail(std::string _detailStr)
{
  detailVector.emplace_back(_detailStr);
  return *this;
}

Exception& Exception::addDetails(const DetailList& _detailList)
{
  auto begin = _detailList.begin();
  auto end = _detailList.end();

  while (begin != end)
    addDetail(*begin++);

  return *this;
}

Exception& Exception::addParameter(const char* _name, std::string _value)
{
  parameterVector.push_back(std::make_pair(std::string(_name), _value));
  return *this;
}

const char* Exception::getDetailByIndex(unsigned int _index) const
{
  if (_index < getDetailCount())
    return detailVector.at(_index).c_str();

  return nullptr;
}

const char* Exception::getParameterNameByIndex(unsigned int _index) const
{
  if (_index < getParameterCount())
  {
    auto p = parameterVector.at(_index);
    return p.first.c_str();
  }

  return nullptr;
}

const char* Exception::getParameterValue(const char* _name) const
{
  size_t size = parameterVector.size();
  if (size > 0)
  {
    for (size_t t = 0; t < size; t++)
    {
      auto p = parameterVector.at(t);
      if (p.first == _name)
        return p.second.c_str();
    }
  }
  return nullptr;
}

const char* Exception::getParameterValueByIndex(unsigned int _index) const
{
  if (_index < getParameterCount())
  {
    auto p = parameterVector.at(_index);
    return p.second.c_str();
  }

  return nullptr;
}

const Exception* Exception::getExceptionByParameterName(const char* _paramName) const
{
  if (getParameterValue(_paramName) != nullptr)
    return this;

  if (prevException == nullptr)
    return nullptr;

  return prevException->getExceptionByParameterName(_paramName);
}

unsigned int Exception::getDetailCount() const
{
  return static_cast<unsigned int>(detailVector.size());
}

unsigned int Exception::getParameterCount() const
{
  return static_cast<unsigned int>(parameterVector.size());
}

bool Exception::loggingDisabled() const
{
  return mLoggingDisabled;
}

bool Exception::stackTraceDisabled() const
{
  return mStackTraceDisabled;
}

Exception& Exception::disableLogging()
{
  mLoggingDisabled = true;
  return *this;
}

Exception& Exception::disableStackTrace()
{
  mStackTraceDisabled = true;
  return *this;
}

std::string Exception::getStackTrace() const
{
  if (mLoggingDisabled || mStackTraceDisabled)
    return "";

  std::ostringstream out;

  const Exception* e = this;

  out << "\n";
  out << ANSI_BG_RED << ANSI_FG_WHITE << ANSI_BOLD_ON << " ##### " << e->timestamp << " ##### "
      << ANSI_BOLD_OFF << ANSI_FG_DEFAULT << ANSI_BG_DEFAULT;
  out << "\n\n";

  while (e != nullptr)
  {
    out << ANSI_FG_RED << ANSI_BOLD_ON << "EXCEPTION " << ANSI_BOLD_OFF << e->message
        << ANSI_FG_DEFAULT << "\n";

    // out << ANSI_BOLD_ON << " * Time       : " << ANSI_BOLD_OFF << e->timestamp << "\n";
    out << ANSI_BOLD_ON << " * Function   : " << ANSI_BOLD_OFF << e->function << "\n";
    out << ANSI_BOLD_ON << " * Location   : " << ANSI_BOLD_OFF << e->filename << ":" << e->line
        << "\n";

    size_t size = e->detailVector.size();
    if (size > 0)
    {
      out << ANSI_BOLD_ON << " * Details    :\n" << ANSI_BOLD_OFF;
      for (size_t t = 0; t < size; t++)
      {
        out << "   - " << e->detailVector.at(t) << "\n";
      }
    }

    size = e->parameterVector.size();
    if (size > 0)
    {
      out << ANSI_BOLD_ON << " * Parameters :\n" << ANSI_BOLD_OFF;
      for (size_t t = 0; t < size; t++)
      {
        auto p = e->parameterVector.at(t);
        out << "   - " << p.first << " = " << p.second << "\n";
      }
    }
    out << "\n";

    e = e->prevException;
  }

  return out.str();
}

std::string Exception::getHtmlStackTrace() const
{
  // This is used only when debugging, hence mStackTraceDisabled is ignored

  std::ostringstream out;

  out << "<html><body>"
      << "<h2>" << timestamp << "</h2>";

  const Exception* e = this;
  while (e != nullptr)
  {
    out << "<h2>" << e->message << "</h2>";
    out << "<ul>"
        << "<li><it>Function :</it>" << e->function << "</li>"
        << "<li><it>Location :</it>" << e->filename << ":" << e->line << "</li>";

    size_t size = e->detailVector.size();
    if (size > 0)
    {
      out << "<li><it>Details :</it></li>";
      out << "<ol>";
      for (size_t t = 0; t < size; t++)
      {
        out << "<li>" << e->detailVector.at(t) << "</li>";
      }
      out << "</ol>";
    }

    size = e->parameterVector.size();
    if (size > 0)
    {
      out << "<li><it>Parameters :</it>";
      out << "<ol>";
      for (size_t t = 0; t < size; t++)
      {
        auto p = e->parameterVector.at(t);
        out << "<li>" << p.first << " = " << p.second << "</li>";
      }
      out << "</ol></li>";
    }
    out << "</ul>";

    e = e->prevException;
  }

  out << "</body></html>";

  return out.str();
}

// ----------------------------------------------------------------------
/*!
 * \brief Print the exception error report while obeying the user settings
 */
// ----------------------------------------------------------------------

void Exception::printError() const
{
  if (!stackTraceDisabled())
    std::cerr << getStackTrace() << std::flush;
  else
  {
    std::cerr << Spine::log_time_str() << " Error: " << what() << std::endl;

    // Print parameters for top level exception, if there are any. Usually
    // plugins set the URI to the top level exception.
    
    if (!parameterVector.empty())
    {
      for(const auto & param_value : parameterVector)
        std::cerr << "   - " << param_value.first << " = " << param_value.second << std::endl;
    }
  }
}

}  // namespace Spine
}  // namespace SmartMet
