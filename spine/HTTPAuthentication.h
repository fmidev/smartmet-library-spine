// ----------------------------------------------------------------------
/*!
 * \brief Functions and classes to handle HTTP request authentication
 */
// ----------------------------------------------------------------------
#pragma once

#include "HTTP.h"
#include <map>
#include <string>

namespace SmartMet
{
namespace Spine
{
namespace HTTP
{
class Authentication
{
 public:
  /**
   *   @brief Constructor
   *
   *   @param denyByDefault specifies what to do if no users are provided using addUser
   *          method (e.g. corresponding data missing from configuration).
   *          - true (the default) - return authentication failure
   *          - false - accept any authentication
   */
  explicit Authentication(bool denyByDefault = true);

  virtual ~Authentication();

  /**
   *  @brief Add user information from configuration
   *
   *   @param name the user name
   *   @param password the user password
   *   @param groupMask a bit mask of user rights
   */
  void addUser(const std::string& name, const std::string& password, unsigned groupMask = 1);

  /**
   *   @brief Removes user with specified name
   *
   *   @param name the user name
   *   @retval false user name not found
   *   @retval true user removed
   */
  bool removeUser(const std::string& name);

  /**
   *   @brief Clear all user data
   */
  void clearUsers();

  /**
   *  @brief Verify authentication of the request
   *
   *  @retval true authentication pass. Response is unchanged and user may fill it
   *  @retval false authentication rejected. Response if filled accordingly (401 unauthorized)
   */
  bool authenticateRequest(const Request& request, Response& response);

 protected:
  /**
   *  @brief Check whether authentication is required for a specified request
   *
   *  Implementation of this base class always returns true
   */
  virtual bool isAuthenticationRequired(const Request& request) const;

  /**
   *  @brief Returns bit mask of user groups for which a specific request is permitted
   */
  virtual unsigned getAuthenticationGroup(const Request& request) const;

  virtual std::string getRealm() const;

  void unauthorizedResponse(Response& response);
  void badRequestResponse(Response& response);

 private:
  bool denyByDefault;
  std::map<std::string, std::pair<std::string, unsigned> > userMap;
};

}  // namespace HTTP
}  // namespace Spine
}  // namespace SmartMet
