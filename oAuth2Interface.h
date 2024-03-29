// File:  oAuth2Interface.h
// Date:  4/15/2013
// Auth:  K. Loux
// Desc:  Handles interface to a server using OAuth 2.0.  Implemented as a thread-safe
//        singleton.

#ifndef OAUTH2_INTERFACE_H_
#define OAUTH2_INTERFACE_H_

// Standard C++ headers
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
	
	void SetLoggingTarget(UString::OStream& newLog) { log = &newLog; }

	void SetAuthenticationURL(const UString::String &authURLIn) { authURL = authURLIn; }
	void SetAuthenticationPollURL(const UString::String &authPollURLIn) { authPollURL = authPollURLIn; }
	void SetTokenURL(const UString::String &tokenURLIn) { tokenURL = tokenURLIn; }
	void SetResponseType(const UString::String &responseTypeIn) { responseType = responseTypeIn; }
	void SetClientID(const UString::String &clientIDIn) { clientID = clientIDIn; }
	void SetClientSecret(const UString::String &clientSecretIn) { clientSecret = clientSecretIn; }
	void SetRedirectURI(const UString::String &redirectURIIn) { redirectURI = redirectURIIn; }
	void SetScope(const UString::String &scopeIn) { scope = scopeIn; }
	void SetLoginHint(const UString::String &loginHintIn) { loginHint = loginHintIn; }
	void SetGrantType(const UString::String &grantTypeIn) { grantType = grantTypeIn; }
	void SetPollGrantType(const UString::String &pollGrantTypeIn) { pollGrantType = pollGrantTypeIn; }

	void SetRefreshToken(const UString::String &refreshTokenIn = UString::String());

	void SetSuccessMessage(const UString::String& message) { successMessage = message; }

	UString::String GetRefreshToken() const { return refreshToken; }
	UString::String GetAccessToken();

	static UString::String Base36Encode(const int64_t &value);

private:
	OAuth2Interface();
	
	UString::OStream* log = &Cout;

	UString::String authURL;
	UString::String authPollURL;
	UString::String tokenURL;
	UString::String responseType;
	UString::String clientID;
	UString::String clientSecret;
	UString::String redirectURI;
	UString::String scope;
	UString::String loginHint;
	UString::String grantType;
	UString::String pollGrantType;

	UString::String refreshToken;
	UString::String accessToken;

	UString::String successMessage = _T("API access successfulley authorized.");

	UString::String RequestRefreshToken();

	UString::String AssembleRefreshRequestQueryString(const UString::String &state = UString::String()) const;
	UString::String AssembleAccessRequestQueryString(const UString::String &code = UString::String(), const bool& usePollGrantType = false) const;

	struct AuthorizationResponse
	{
		UString::String deviceCode;
		double expiresIn;
		int interval;// [sec]
	};

	bool HandleAuthorizationRequestResponse(const UString::String &buffer,
		AuthorizationResponse &response);
	bool HandleRefreshRequestResponse(const UString::String &buffer, const bool &silent = false);
	bool HandleAccessRequestResponse(const UString::String &buffer);
	bool ResponseContainsError(const UString::String &buffer);

	UString::String GenerateSecurityStateKey() const;
	bool RedirectURIIsLocal() const;
	bool IsLimitedInput() const { return redirectURI.empty(); }
	unsigned short StripPortFromLocalRedirectURI() const;
	UString::String StripAddressFromLocalRedirectURI() const;

	std::chrono::system_clock::time_point accessTokenValidUntilTime;

	static OAuth2Interface *singleton;
	static UString::String ExtractAuthCodeFromGETRequest(const std::string& rawRequest);

	static std::string BuildHTTPSuccessResponse(const UString::String& successMessage);
};

#endif// OAUTH2_INTERFACE_H_
