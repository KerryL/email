// File:  oAuth2Interface.h
// Date:  4/15/2013
// Auth:  K. Loux
// Desc:  Handles interface to a server using OAuth 2.0.  Implemented as a thread-safe
//        singleton.

#ifndef OAUTH2_INTERFACE_H_
#define OAUTH2_INTERFACE_H_

// Standard C++ headers
#include <string>
#include <chrono>

// email headers
#include "jsonInterface.h"

// utilities headers
#include "utilities/uString.h"

// cJSON (local) forward declarations
struct cJSON;

class OAuth2Interface : public JSONInterface
{
public:

	static OAuth2Interface& Get();
	static void Destroy();

	void SetAuthenticationURL(const String &authURLIn) { authURL = authURLIn; }
	void SetTokenURL(const String &tokenURLIn) { tokenURL = tokenURLIn; }
	void SetResponseType(const String &responseTypeIn) { responseType = responseTypeIn; }
	void SetClientID(const String &clientIDIn) { clientID = clientIDIn; }
	void SetClientSecret(const String &clientSecretIn) { clientSecret = clientSecretIn; }
	void SetRedirectURI(const String &redirectURIIn) { redirectURI = redirectURIIn; }
	void SetScope(const String &scopeIn) { scope = scopeIn; }
	void SetLoginHint(const String &loginHintIn) { loginHint = loginHintIn; }
	void SetGrantType(const String &grantTypeIn) { grantType = grantTypeIn; }
	
	void SetRefreshToken(const String &refreshTokenIn = String());

	String GetRefreshToken() const { return refreshToken; }
	String GetAccessToken();

	static String Base36Encode(const int64_t &value);

private:
	OAuth2Interface();
	~OAuth2Interface();

	String authURL, tokenURL;
	String responseType;
	String clientID;
	String clientSecret;
	String redirectURI;
	String scope;
	String loginHint;
	String grantType;

	String refreshToken;
	String accessToken;

	String RequestRefreshToken();

	String AssembleRefreshRequestQueryString(const String &state = String()) const;
	String AssembleAccessRequestQueryString(const String &code = String()) const;

	struct AuthorizationResponse
	{
		String deviceCode;
		double expiresIn;
		int interval;
	};

	bool HandleAuthorizationRequestResponse(const String &buffer,
		AuthorizationResponse &response);
	bool HandleRefreshRequestResponse(const String &buffer, const bool &silent = false);
	bool HandleAccessRequestResponse(const String &buffer);
	bool ResponseContainsError(const String &buffer);

	String GenerateSecurityStateKey() const;
	bool RedirectURIIsLocal() const;
	bool IsLimitedInput() { return redirectURI.empty(); };
	int StripPortFromLocalRedirectURI() const;

	std::chrono::system_clock::time_point accessTokenValidUntilTime;

	static OAuth2Interface *singleton;
};

#endif// OAUTH2_INTERFACE_H_