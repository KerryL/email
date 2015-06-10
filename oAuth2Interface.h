// File:  oAuth2Interface.h
// Date:  4/15/2013
// Auth:  K. Loux
// Desc:  Handles interface to a server using OAuth 2.0.  Implemented as a thread-safe
//        singleton.

#ifndef OAUTH2_INTERFACE_H_
#define OAUTH2_INTERFACE_H_

// Standard C++ headers
#include <string>
#include <ctime>

// utilities headers
#include "utilities/portable.h"

// cJSON (local) forward declarations
struct cJSON;

class OAuth2Interface
{
public:

	static OAuth2Interface& Get(void);
	static void Destroy(void);

	void SetAuthenticationURL(const std::string &authURL) { this->authURL = authURL; }
	void SetTokenURL(const std::string &tokenURL) { this->tokenURL = tokenURL; }
	void SetResponseType(const std::string &responseType) { this->responseType = responseType; }
	void SetClientID(const std::string &clientID) { this->clientID = clientID; }
	void SetClientSecret(const std::string &clientSecret) { this->clientSecret = clientSecret; }
	void SetRedirectURI(const std::string &redirectURI) { this->redirectURI = redirectURI; }
	void SetScope(const std::string &scope) { this->scope = scope; }
	void SetLoginHint(const std::string &loginHint) { this->loginHint = loginHint; }
	void SetGrantType(const std::string &grantType) { this->grantType = grantType; }
	void SetVerboseOutput(const bool &verbose = true) { this->verbose = verbose; }
	void SetCertificatePath(const std::string &caCertificatePath) { this->caCertificatePath = caCertificatePath; }
	
	void SetRefreshToken(const std::string &refreshToken = "");

	std::string GetRefreshToken(void) const { return refreshToken; }
	std::string GetAccessToken(void);

	static std::string Base36Encode(const LongLong &value);

private:
	OAuth2Interface();
	~OAuth2Interface();

	std::string authURL, tokenURL;
	std::string responseType;
	std::string clientID;
	std::string clientSecret;
	std::string redirectURI;
	std::string scope;
	std::string loginHint;
	std::string grantType;

	std::string refreshToken;
	std::string accessToken;

	std::string caCertificatePath;

	bool verbose;

	std::string RequestRefreshToken(void);

	std::string AssembleRefreshRequestQueryString(const std::string &state = "") const;
	std::string AssembleAccessRequestQueryString(const std::string &code = "") const;

	struct AuthorizationResponse
	{
		std::string deviceCode;
		double expiresIn;
		int interval;
	};

	bool DoCURLPost(const std::string &url, const std::string &data,
		std::string &response) const;

	bool HandleAuthorizationRequestResponse(const std::string &buffer,
		AuthorizationResponse &response);
	bool HandleRefreshRequestResponse(const std::string &buffer, const bool &silent = false);
	bool HandleAccessRequestResponse(const std::string &buffer);
	bool ResponseContainsError(const std::string &buffer);

	std::string GenerateSecurityStateKey(void) const;
	bool RedirectURIIsLocal(void) const;
	bool IsLimitedInput(void) { return redirectURI.empty(); };
	int StripPortFromLocalRedirectURI(void) const;

	bool ReadJSON(cJSON *root, std::string field, int &value) const;
	bool ReadJSON(cJSON *root, std::string field, std::string &value) const;
	bool ReadJSON(cJSON *root, std::string field, double &value) const;

	static size_t CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData);

	// timing information to determine if we need to request a new code

	time_t accessTokenObtainedTime;
	double accessTokenValidTime;// [sec]

	static OAuth2Interface *singleton;
};

#endif// OAUTH2_INTERFACE_H_
