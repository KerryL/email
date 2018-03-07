// File:  oAuth2Interface.cpp
// Date:  4/15/2013
// Auth:  K. Loux
// Desc:  Handles interface to a server using OAuth 2.0.

// Standard C++ headers
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <sstream>
#include <ctime>
#include <algorithm>

// Standard C headers
#include <string.h>

// *nix headers
#ifndef _WIN32
#include <unistd.h>
#endif

// cURL headers
#include <curl/curl.h>

// Local headers
#include "oAuth2Interface.h"

//==========================================================================
// Class:			OAuth2Interface
// Function:		OAuth2Interface
//
// Description:		Constant declarations
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		None
//
//==========================================================================
OAuth2Interface *OAuth2Interface::singleton = NULL;

//==========================================================================
// Class:			OAuth2Interface
// Function:		OAuth2Interface
//
// Description:		Constructor for OAuth2Interface class.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		None
//
//==========================================================================
OAuth2Interface::OAuth2Interface()
{
	verbose = false;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		~OAuth2Interface
//
// Description:		Destructor for OAuth2Interface class.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		None
//
//==========================================================================
OAuth2Interface::~OAuth2Interface()
{
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		Get (static)
//
// Description:		Access method for singleton pattern.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		None
//
//==========================================================================
OAuth2Interface& OAuth2Interface::Get()
{
	if (!singleton)
		singleton = new OAuth2Interface;

	return *singleton;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		Destroy (static)
//
// Description:		Cleans up memory associated with singleton pattern.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		None
//
//==========================================================================
void OAuth2Interface::Destroy()
{
	if (singleton)
		delete singleton;

	singleton = NULL;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		SetRefreshToken
//
// Description:		Sets the refresh token.  If the token is not valid, request
//					information from the user to obtain a valid token.
//
// Input Arguments:
//		refreshToken	= const String&
//
// Output Arguments:
//		None
//
// Return Value:
//		None
//
//==========================================================================
void OAuth2Interface::SetRefreshToken(const String &refreshTokenIn)
{
	// If the token isn't valid, request one, otherwise, use it as-is
	if (refreshTokenIn.length() < 2)// TODO:  Better way to tell if it's valid?
		refreshToken = RequestRefreshToken();// TODO:  Check for errors (returned empty string?)
	else
		refreshToken = refreshTokenIn;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		RequestRefreshToken
//
// Description:		Requests a refresh token from the server.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		String containing refresh token, or emtpy string on error
//
//==========================================================================
String OAuth2Interface::RequestRefreshToken()
{
	assert(!authURL.empty() &&
		!tokenURL.empty());

	if (IsLimitedInput())
	{
		std::string rawReadBuffer;
		if (!DoCURLPost(authURL, UString::ToNarrowString(AssembleRefreshRequestQueryString()), rawReadBuffer))
			return String();

		const String readBuffer(UString::ToStringType(rawReadBuffer));

		if (ResponseContainsError(readBuffer))
			return String();

		AuthorizationResponse authResponse;
		if (!HandleAuthorizationRequestResponse(readBuffer, authResponse))
			return String();

		String queryString = AssembleAccessRequestQueryString(authResponse.deviceCode);

		time_t startTime = time(NULL);
		time_t now = startTime;
		while (!HandleRefreshRequestResponse(readBuffer, true))
		{
#ifdef _WIN32
			Sleep(1000 * authResponse.interval);
#else
			sleep(authResponse.interval);
#endif
			now = time(NULL);
			if (difftime(now, startTime) > authResponse.expiresIn)
			{
				Cerr << "Request timed out - restart application to start again\n";
				return String();
			}

			if (!DoCURLPost(tokenURL, UString::ToNarrowString(queryString), rawReadBuffer))
				return String();

			if (ResponseContainsError(readBuffer))
				return String();
		}
	}
	else
	{
		assert(!responseType.empty());

		String stateKey;// = GenerateSecurityStateKey();// Not sure why it doesn't work with the state key...
		
		// TODO:  Alternatively, we can pop up a browser, wait for the user
		// to verify permissions, then grab the result ourselves, without
		// requiring the user to copy/paste into browser, then again to this
		// application.
		// The benefit of doing it the way we're doing it now, though, is
		// that the browser used to authenticate does not need to be on the
		// same machine that is running this application.
		Cout << "Enter this address in your browser:" << std::endl
			<< authURL << "?" << AssembleRefreshRequestQueryString(stateKey) << std::endl;

		String authorizationCode;
		if (RedirectURIIsLocal())
		{
			// TODO:  What now?  Need to receive authorization code by listening to appropriate port?
			// Can cURL do this?
			//curl_easy_setopt(curl, CURLOPT_PORT, StripPortFromLocalRedirectURI());
			assert(false);
		}
		else
		{
			// TODO:  Grab verification code automatically (see note above
			// prior to "Enter this address...")
			Cout << "Enter verification code:" << std::endl;
			Cin >> authorizationCode;
		}

		std::string readBuffer;
		if (!DoCURLPost(tokenURL, UString::ToNarrowString(AssembleAccessRequestQueryString(authorizationCode)), readBuffer) ||
			ResponseContainsError(UString::ToStringType(readBuffer)) ||
			!HandleRefreshRequestResponse(UString::ToStringType(readBuffer)))
		{
			Cerr << "Failed to obtain refresh token\n";
			return String();
		}
	}

	Cout << "Successfully obtained refresh token" << std::endl;
	return refreshToken;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		ResponseContainsError
//
// Description:		Checks JSON array to see if there is an error entry.
//					"Authorization pending" errors are not considered errors.
//
// Input Arguments:
//		buffer		= const String & containing JSON string
//
// Output Arguments:
//		None
//
// Return Value:
//		bool, true for error, false otherwise
//
//==========================================================================
bool OAuth2Interface::ResponseContainsError(const String &buffer)
{
	cJSON *root(cJSON_Parse(UString::ToNarrowString(buffer).c_str()));
	if (!root)
	{
		Cerr << "Failed to parse returned string (ResponseContainsError())\n";
		if (verbose)
			Cerr << buffer << '\n';
		return true;
	}

	String error;
	if (ReadJSON(root, _T("error"), error))
	{
		if (error.compare(_T("authorization_pending")) != 0)
		{
			Cerr << "Recieved error from OAuth server:  " << error;
			String description;
			if (ReadJSON(root, _T("error_description"), description))
				Cerr << " - " << description;
			Cerr << '\n';
			cJSON_Delete(root);
			return true;
		}

		cJSON_Delete(root);
	}

	return false;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		HandleAuthorizationRequestResponse
//
// Description:		Processes JSON responses from server.  Used for input-limited
//					devices only.
//
// Input Arguments:
//		buffer		= const String & containing JSON string
//
// Output Arguments:
//		response	= AuthorizationResponse&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool OAuth2Interface::HandleAuthorizationRequestResponse(
	const String &buffer, AuthorizationResponse &response)
{
	assert(IsLimitedInput());

	cJSON *root(cJSON_Parse(UString::ToNarrowString(buffer).c_str()));
	if (!root)
	{
		Cerr << "Failed to parse returned string (HandleAuthorizationRequestResponse())\n";
		return false;
	}

	String userCode, verificationURL;

	// TODO:  Check state key?
	if (!ReadJSON(root, _T("device_code"), response.deviceCode) ||
		!ReadJSON(root, _T("user_code"), userCode) ||
		!ReadJSON(root, _T("verification_url"), verificationURL) ||
		!ReadJSON(root, _T("expires_in"), response.expiresIn) ||
		!ReadJSON(root, _T("interval"), response.interval))
	{
		cJSON_Delete(root);
		return false;
	}

	Cout << "Please visit this URL: " << std::endl << verificationURL << std::endl;
	Cout << "And enter this code (case sensitive):" << std::endl << userCode << std::endl;

	cJSON_Delete(root);
	return true;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		HandleRefreshRequestResponse
//
// Description:		Processes JSON responses from server.
//
// Input Arguments:
//		buffer	= const String & containing JSON string
//
// Output Arguments:
//		None
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool OAuth2Interface::HandleRefreshRequestResponse(const String &buffer, const bool &silent)
{
	cJSON *root = cJSON_Parse(UString::ToNarrowString(buffer).c_str());
	if (!root)
	{
		if (!silent)
			Cerr << "Failed to parse returned string (HandleRefreshRequsetResponse())\n";
		return false;
	}

	String tokenType;
	if (!ReadJSON(root, _T("refresh_token"), refreshToken))
	{
		if (!silent)
			Cerr << "Failed to read refresh token field from server" << std::endl;
		cJSON_Delete(root);
		return false;
	}
	cJSON_Delete(root);

	return HandleAccessRequestResponse(buffer);
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		HandleAccessRequestResponse
//
// Description:		Processes JSON responses from server.
//
// Input Arguments:
//		buffer	= const String & containing JSON string
//
// Output Arguments:
//		None
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool OAuth2Interface::HandleAccessRequestResponse(const String &buffer)
{
	cJSON *root = cJSON_Parse(UString::ToNarrowString(buffer).c_str());
	if (!root)
	{
		Cerr << "Failed to parse returned string (HandleAccessRequestResponse())\n";
		return false;
	}

	String tokenType;
	unsigned int tokenValidDuration;// [sec]
	if (!ReadJSON(root, _T("access_token"), accessToken) ||
		!ReadJSON(root, _T("token_type"), tokenType) ||
		!ReadJSON(root, _T("expires_in"), tokenValidDuration))
	{
		Cerr << "Failed to read all required fields from server\n";
		cJSON_Delete(root);
		return false;
	}

	if (tokenType.compare(_T("Bearer")) != 0)
	{
		Cerr << "Expected token type 'Bearer', received '" << tokenType << "'\n";
		cJSON_Delete(root);
		return false;
	}

	accessTokenValidUntilTime = std::chrono::system_clock::now() + std::chrono::seconds(tokenValidDuration);

	cJSON_Delete(root);
	return true;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		GetAccessToken
//
// Description:		Returns a valid access token.  This method generates new
//					access tokens as necessary (as they expire).
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		String containing access token (or empty string on error)
//
//==========================================================================
String OAuth2Interface::GetAccessToken()
{
	// TODO:  Better way to check if access token is valid?  It would be good to be able
	// to request a new one after an API response with a 401 error.
	if (!accessToken.empty() && std::chrono::system_clock::now() < accessTokenValidUntilTime)
		return accessToken;

	Cout << "Access token is invalid - requesting a new one" << std::endl;

	std::string readBuffer;
	if (!DoCURLPost(tokenURL, UString::ToNarrowString(AssembleAccessRequestQueryString()), readBuffer) ||
		ResponseContainsError(UString::ToStringType(readBuffer)) ||
		!HandleAccessRequestResponse(UString::ToStringType(readBuffer)))
	{
		Cerr << "Failed to obtain access token" << std::endl;
		return String();
	}

	Cout << "Successfully obtained new access token" << std::endl;
	return accessToken;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		AssembleRefreshRequestQueryString
//
// Description:		Assembles the proper request query string for obtaining a
//					refresh token.
//
// Input Arguments:
//		state	= const String&, anti-forgery state key
//
// Output Arguments:
//		None
//
// Return Value:
//		String containing access token (or empty string on error)
//
//==========================================================================
String OAuth2Interface::AssembleRefreshRequestQueryString(const String& state) const
{
	assert(!clientID.empty() &&
		!scope.empty());

	// Required fields
	String queryString;
	queryString.append(_T("client_id=") + clientID);
	queryString.append(_T("&scope=") + scope);

	// Optional fields
	if (!loginHint.empty())
		queryString.append(_T("&login_hint=") + loginHint);

	if (!responseType.empty())
		queryString.append(_T("&response_type=") + responseType);

	if (!redirectURI.empty())
		queryString.append(_T("&redirect_uri=") + redirectURI);

	if (!state.empty())
		queryString.append(_T("&state=") + state);

	return queryString;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		AssembleAccessRequestQueryString
//
// Description:		Assembles the proper request query string for obtaining an access
//					token.
//
// Input Arguments:
//		code	= const String&
//
// Output Arguments:
//		None
//
// Return Value:
//		String containing access token (or empty string on error)
//
//==========================================================================
String OAuth2Interface::AssembleAccessRequestQueryString(const String &code) const
{
	assert((!refreshToken.empty() || !code.empty()) &&
		!clientID.empty() &&
		!clientSecret.empty()/* &&
		!grantType.empty()*/);

	// Required fields
	String queryString;
	queryString.append(_T("client_id=") + clientID);
	queryString.append(_T("&client_secret=") + clientSecret);

	if (code.empty())
	{
		queryString.append(_T("&refresh_token=") + refreshToken);
		queryString.append(_T("&grant_type=refresh_token"));
	}
	else
	{
		queryString.append(_T("&code=") + code);
		queryString.append(_T("&grant_type=") + grantType);
		if (!redirectURI.empty())
			queryString.append(_T("&redirect_uri=") + redirectURI);
	}

	return queryString;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		RedirectURIIsLocal
//
// Description:		Checks to see if the URI indicates we should be listening
//					on a local port.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		bool
//
//==========================================================================
bool OAuth2Interface::RedirectURIIsLocal() const
{
	assert(!redirectURI.empty());
	const String localURL(_T("http://localhost"));

	return redirectURI.substr(0, localURL.length()).compare(localURL) == 0;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		StripPortFromLocalRedirectURI
//
// Description:		Parses the redirect URI string to obtain the local port number.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		int, contains port number or zero for error
//
//==========================================================================
int OAuth2Interface::StripPortFromLocalRedirectURI() const
{
	assert(RedirectURIIsLocal());

	size_t colon = redirectURI.find(':');
	if (colon == String::npos)
		return 0;

	IStringStream s(redirectURI.substr(colon + 1));

	int port;
	s >> port;
	return port;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		GenerateSecurityStateKey
//
// Description:		Generates a random string of characters to use as a
//					security state key.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		String
//
//==========================================================================
String OAuth2Interface::GenerateSecurityStateKey() const
{
	String stateKey;
	while (stateKey.length() < 30)
		stateKey.append(Base36Encode((int64_t)rand()
			* (int64_t)rand() * (int64_t)rand() * (int64_t)rand()));

	return stateKey;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		Base36Encode
//
// Description:		Encodes the specified value in base36.
//
// Input Arguments:
//		value	= const int64_t&
//
// Output Arguments:
//		None
//
// Return Value:
//		String
//
//==========================================================================
String OAuth2Interface::Base36Encode(const int64_t &value)
{
	const unsigned int maxDigits(35);
	const char* charset = "abcdefghijklmnopqrstuvwxyz0123456789";
	String buf;
	buf.reserve(maxDigits);

	int64_t v(value);

	do
	{
		buf += charset[std::abs(v % 36)];
		v /= 36;
	} while (v);

	std::reverse(buf.begin(), buf.end());

	return buf;
}