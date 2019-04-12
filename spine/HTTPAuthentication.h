// ----------------------------------------------------------------------
/*!
 * \brief Functions and classes to handle HTTP request authentication
 */
// ----------------------------------------------------------------------
#pragma once

#include <map>
#include <string>
#include "HTTP.h"

namespace SmartMet {
  namespace Spine {
    namespace HTTP {

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
	Authentication(bool denyByDefault = true);

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
	 *  @brief Verify authentication of the request
	 *
	 *  @retval true authetication pass. Response is unchanged and user may fill it
	 *  @retval false authentication rejected. Response if filled accordingly (401 unauthorized)
	 */
	bool autheticateRequest(const Request& request, Response& response);

      protected:

	/**
	 *  @brief Check whether authetication is required for a specified request
	 *
	 *  Implementation of this base class always returns true
	 */
	virtual bool isAutheticationRequired(const Request& request) const;

	/**
	 *  @brief Returns bit mask of user groups for which a specific request is permitted
	 */
	virtual unsigned getAutheticationGroup(const Request& request) const;

	virtual std::string getRealm() const;

	void unauthorizedResponse(Response& response);
	void badRequestResponse(Response& response);

      private:
	bool denyByDefault;
	std::map<std::string, std::pair<std::string, unsigned> > userMap;
      };

    } // namespace HTTP
  } // namespace Spine
} // namespace SmartMet

