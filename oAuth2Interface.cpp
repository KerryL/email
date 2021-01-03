// File:  oAuth2Interface.cpp
// Date:  4/15/2013
// Auth:  K. Loux
// Desc:  Handles interface to a server using OAuth 2.0.

// Standard C++ headers
#include <cstdlib>
#include <cassert>
#include <sstream>
#include <ctime>
#include <algorithm>

// Standard C headers
#include <string.h>

// cURL headers
#include <curl/curl.h>

// utilities headers
#include "utilities/cppSocket.h"

// OS headers
#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif

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
OAuth2Interface *OAuth2Interface::singleton = nullptr;

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

	singleton = nullptr;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		SetRefreshToken
//
// Description:		Sets the refresh token.  If the token is not valid, request
//					information from the user to obtain a valid token.
//
// Input Arguments:
//		refreshToken	= const UString::String&
//
// Output Arguments:
//		None
//
// Return Value:
//		None
//
//==========================================================================
void OAuth2Interface::SetRefreshToken(const UString::String &refreshTokenIn)
{
	// If the token isn't valid, request one, otherwise, use it as-is
	if (refreshTokenIn.length() < 2)// TODO:  Better way to tell if it's valid?
		refreshToken = RequestRefreshToken();// TODO:  Check for errors (returned empty UString::?)
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
//		UString::String containing refresh token, or emtpy UString::String on error
//
//==========================================================================
UString::String OAuth2Interface::RequestRefreshToken()
{
	assert(!authURL.empty() && !tokenURL.empty());

	if (IsLimitedInput())
	{
		std::string rawReadBuffer;
		if (!DoCURLPost(authURL, UString::ToNarrowString(AssembleRefreshRequestQueryString()), rawReadBuffer))
			return UString::String();

		UString::String readBuffer(UString::ToStringType(rawReadBuffer));

		if (ResponseContainsError(readBuffer))
			return UString::String();

		AuthorizationResponse authResponse;
		if (!HandleAuthorizationRequestResponse(readBuffer, authResponse))
			return UString::String();

		UString::String queryString = AssembleAccessRequestQueryString(authResponse.deviceCode, true);

		time_t startTime = time(nullptr);
		time_t now = startTime;
		while (!HandleRefreshRequestResponse(readBuffer, true))
		{
#ifdef _WIN32
			Sleep(1000 * authResponse.interval);
#else
			sleep(authResponse.interval);
#endif
			now = time(nullptr);
			if (difftime(now, startTime) > authResponse.expiresIn)
			{
				*log << "Request timed out - restart application to start again" << std::endl;
				return UString::String();
			}

			if (!DoCURLPost(authPollURL, UString::ToNarrowString(queryString), rawReadBuffer))
				return UString::String();
			readBuffer = UString::ToStringType(rawReadBuffer);

			if (ResponseContainsError(readBuffer))
				return UString::String();
		}
	}
	else
	{
		assert(!responseType.empty());

		UString::String stateKey;// = GenerateSecurityStateKey();// Not sure why it doesn't work with the state key...

		const UString::String assembledAuthURL(authURL + UString::Char('?') + AssembleRefreshRequestQueryString(stateKey));
		CPPSocket webSocket(CPPSocket::SocketType::SocketTCPServer);
		if (RedirectURIIsLocal())
		{
			if (!webSocket.Create(StripPortFromLocalRedirectURI(), UString::ToNarrowString(StripAddressFromLocalRedirectURI()).c_str()))
				return UString::String();
#ifdef _WIN32
			ShellExecute(nullptr, _T("open"), assembledAuthURL.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
			system(UString::ToNarrowString(UString::String(_T("xdg-open '")) + assembledAuthURL + UString::String(_T("'"))).c_str());
#endif
		}
		else// (for example, with redirect URI set to "oob")
		{
			// The benefit of doing it the way we're doing it now, though, is
			// that the browser used to authenticate does not need to be on the
			// same machine that is running this application.
			Cout << "Enter this address in your browser:" << std::endl << assembledAuthURL << std::endl;
		}

		UString::String authorizationCode;
		if (RedirectURIIsLocal())
		{
			if (!webSocket.WaitForClientData(60000))
			{
				*log << "No response... aborting" << std::endl;
				return UString::String();
			}

			std::string message;
			{
				std::lock_guard<std::mutex> lock(webSocket.GetMutex());
				const auto messageSize(webSocket.Receive());
				if (messageSize <= 0)
					return UString::String();
				message.assign(reinterpret_cast<char*>(webSocket.GetLastMessage()), messageSize);
			}

			if (message.empty())
				return UString::String();

			authorizationCode = ExtractAuthCodeFromGETRequest(message);
			const auto successResponse(BuildHTTPSuccessResponse(successMessage));
			assert(successResponse.length() < std::numeric_limits<unsigned int>::max());
			if (!webSocket.TCPSend(reinterpret_cast<const CPPSocket::DataType*>(successResponse.c_str()), static_cast<int>(successResponse.length())))
				*log << "Warning:  Authorization code response failed to send" << std::endl;
		}
		else
		{
			Cout << "Enter verification code:" << std::endl;
			Cin >> authorizationCode;
		}

		std::string readBuffer;
		if (!DoCURLPost(tokenURL, UString::ToNarrowString(AssembleAccessRequestQueryString(authorizationCode)), readBuffer) ||
			ResponseContainsError(UString::ToStringType(readBuffer)) ||
			!HandleRefreshRequestResponse(UString::ToStringType(readBuffer)))
		{
			*log << "Failed to obtain refresh token" << std::endl;
			return UString::String();
		}
	}

	*log << "Successfully obtained refresh token" << std::endl;
	return refreshToken;
}

UString::String OAuth2Interface::ExtractAuthCodeFromGETRequest(const std::string& rawRequest)
{
	UString::String request(UString::ToStringType(rawRequest));
	const UString::String startKey(_T("?code="));
	const auto start(request.find(startKey));
	if (start == UString::String::npos)
		return UString::String();

	const UString::String endKey(_T(" HTTP/1.1"));
	const auto end(request.find(endKey, start));
	if (end == UString::String::npos)
		return UString::String();
	return request.substr(start + startKey.length(), end - start - startKey.length());
}

std::string OAuth2Interface::BuildHTTPSuccessResponse(const UString::String& successMessage)
{
	std::string body("<html><body><h1>Success!</h1><p>" + UString::ToNarrowString(successMessage) + "</p></body></html>");

	std::ostringstream headerStream;
	headerStream << "HTTP/1.1 200 OK\n"
		<< "Date: Sun, 18 Oct 2009 08:56:53 GMT\n"
		<< "Server: eBirdDataProcessor\n"
		<< "Last-Modified: Sat, 20 Nov 2004 07:16:26 GMT\n"
		<< "Accept-Ranges: bytes\n"
		<< "Content-Length: " << body.length() << '\n'
		<< "Connection: close\n"
		<< "Content-Type: text/html\n\n";

	return headerStream.str() + body;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		ResponseContainsError
//
// Description:		Checks JSON array to see if there is an error entry.
//					"Authorization pending" errors are not considered errors.
//
// Input Arguments:
//		buffer		= const UString::String & containing JSON UString::
//
// Output Arguments:
//		None
//
// Return Value:
//		bool, true for error, false otherwise
//
//==========================================================================
bool OAuth2Interface::ResponseContainsError(const UString::String &buffer)
{
	cJSON *root(cJSON_Parse(UString::ToNarrowString(buffer).c_str()));
	if (!root)
	{
		*log << "Failed to parse returned string (ResponseContainsError())" << std::endl;
		if (verbose)
			Cerr << buffer << '\n';
		return true;
	}

	UString::String error;
	if (ReadJSON(root, _T("error"), error))
	{
		if (error != _T("authorization_pending"))
		{
			UString::OStringStream ss;
			ss << "Recieved error from OAuth server:  " << error;
			UString::String description;
			if (ReadJSON(root, _T("error_description"), description))
				ss << " - " << description;
			*log << ss.str() << std::endl;
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
//		buffer		= const UString::String & containing JSON UString::
//
// Output Arguments:
//		response	= AuthorizationResponse&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool OAuth2Interface::HandleAuthorizationRequestResponse(
	const UString::String &buffer, AuthorizationResponse &response)
{
	assert(IsLimitedInput());

	cJSON *root(cJSON_Parse(UString::ToNarrowString(buffer).c_str()));
	if (!root)
	{
		*log << "Failed to parse returned string (HandleAuthorizationRequestResponse())" << std::endl;
		return false;
	}

	UString::String userCode, verificationURL;

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
//		buffer	= const UString::String& containing JSON
//
// Output Arguments:
//		None
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool OAuth2Interface::HandleRefreshRequestResponse(const UString::String &buffer, const bool &silent)
{
	cJSON *root = cJSON_Parse(UString::ToNarrowString(buffer).c_str());
	if (!root)
	{
		if (!silent)
			*log << "Failed to parse returned string (HandleRefreshRequsetResponse())" << std::endl;
		return false;
	}

	UString::String tokenType;
	if (!ReadJSON(root, _T("refresh_token"), refreshToken))
	{
		if (!silent)
			*log << "Failed to read refresh token field from server" << std::endl;
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
//		buffer	= const UString::String & containing JSON UString::
//
// Output Arguments:
//		None
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool OAuth2Interface::HandleAccessRequestResponse(const UString::String &buffer)
{
	cJSON *root = cJSON_Parse(UString::ToNarrowString(buffer).c_str());
	if (!root)
	{
		*log << "Failed to parse returned string (HandleAccessRequestResponse())" << std::endl;
		return false;
	}

	UString::String tokenType;
	unsigned int tokenValidDuration;// [sec]
	if (!ReadJSON(root, _T("access_token"), accessToken) ||
		!ReadJSON(root, _T("token_type"), tokenType) ||
		!ReadJSON(root, _T("expires_in"), tokenValidDuration))
	{
		*log << "Failed to read all required fields from server" << std::endl;
		cJSON_Delete(root);
		return false;
	}

	if (tokenType.compare(_T("Bearer")) != 0)
	{
		*log << "Expected token type 'Bearer', received '" << tokenType << "'" << std::endl;
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
//		UString::String containing access token (or empty UString::String on error)
//
//==========================================================================
UString::String OAuth2Interface::GetAccessToken()
{
	// TODO:  Better way to check if access token is valid?  It would be good to be able
	// to request a new one after an API response with a 401 error.
	if (!accessToken.empty() && std::chrono::system_clock::now() < accessTokenValidUntilTime)
		return accessToken;

	*log << "Access token is invalid - requesting a new one" << std::endl;

	std::string readBuffer;
	if (!DoCURLPost(tokenURL, UString::ToNarrowString(AssembleAccessRequestQueryString()), readBuffer) ||
		ResponseContainsError(UString::ToStringType(readBuffer)) ||
		!HandleAccessRequestResponse(UString::ToStringType(readBuffer)))
	{
		*log << "Failed to obtain access token" << std::endl;
		return UString::String();
	}

	*log << "Successfully obtained new access token" << std::endl;
	return accessToken;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		AssembleRefreshRequestQueryString
//
// Description:		Assembles the proper request query UString::String for obtaining a
//					refresh token.
//
// Input Arguments:
//		state	= const UString::String&, anti-forgery state key
//
// Output Arguments:
//		None
//
// Return Value:
//		UString::String containing access token (or empty UString::String on error)
//
//==========================================================================
UString::String OAuth2Interface::AssembleRefreshRequestQueryString(const UString::String& state) const
{
	assert(!clientID.empty() &&
		!scope.empty());

	// Required fields
	UString::String queryString;
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
// Description:		Assembles the proper request query UString::String for obtaining an access
//					token.
//
// Input Arguments:
//		code				= const UString::String&
//		usePollGrantType	= const bool&
//
// Output Arguments:
//		None
//
// Return Value:
//		String containing access token (or empty UString::String on error)
//
//==========================================================================
UString::String OAuth2Interface::AssembleAccessRequestQueryString(const UString::String &code, const bool& usePollGrantType) const
{
	assert((!refreshToken.empty() || !code.empty()) &&
		!clientID.empty() &&
		!clientSecret.empty()/* &&
		!grantType.empty()*/);

	// Required fields
	UString::String queryString;
	queryString.append(_T("client_id=") + clientID);
	queryString.append(_T("&client_secret=") + clientSecret);

	if (code.empty())
	{
		queryString.append(_T("&refresh_token=") + refreshToken);
		queryString.append(_T("&grant_type=refresh_token"));
	}
	else
	{
		if (IsLimitedInput())
			queryString.append(_T("&device_code=") + code);
		else
			queryString.append(_T("&code=") + code);
			
		if (usePollGrantType)
		{
			assert(!pollGrantType.empty());
			queryString.append(_T("&grant_type=") + pollGrantType);
		}
		else
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
	const UString::String localURL1(_T("http://localhost"));
	const UString::String localURL2(_T("http://127.0.0.1"));

	return redirectURI.substr(0, localURL1.length()).compare(localURL1) == 0 ||
		redirectURI.substr(0, localURL2.length()).compare(localURL2) == 0;
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		StripPortFromLocalRedirectURI
//
// Description:		Parses the redirect URI UString::String to obtain the local port number.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		unsigned short, contains port number or zero for error
//
//==========================================================================
unsigned short OAuth2Interface::StripPortFromLocalRedirectURI() const
{
	assert(RedirectURIIsLocal());

	const size_t colon(redirectURI.find_last_of(':'));
	if (colon == UString::String::npos)
		return 0;

	UString::IStringStream s(redirectURI.substr(colon + 1));

	unsigned short port;
	s >> port;
	return port;
}


UString::String OAuth2Interface::StripAddressFromLocalRedirectURI() const
{
	assert(RedirectURIIsLocal());

	const size_t colon(redirectURI.find_last_of(':'));
	if (colon == UString::String::npos)
		return redirectURI;
	return redirectURI.substr(0, colon);
}

//==========================================================================
// Class:			OAuth2Interface
// Function:		GenerateSecurityStateKey
//
// Description:		Generates a random UString::String of characters to use as a
//					security state key.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		UString::String
//
//==========================================================================
UString::String OAuth2Interface::GenerateSecurityStateKey() const
{
	UString::String stateKey;
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
//		UString::String
//
//==========================================================================
UString::String OAuth2Interface::Base36Encode(const int64_t &value)
{
	const unsigned int maxDigits(35);
	const char* charset = "abcdefghijklmnopqrstuvwxyz0123456789";
	UString::String buf;
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
