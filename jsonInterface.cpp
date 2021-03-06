// File:  jsonInterface.cpp
// Date:  7/7/2016
// Auth:  K. Loux
// Desc:  Class for interfacing with a remote server using JSON and CURL.

// Standard C++ headers
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <iomanip>

// Standard C headers
#include <string.h>

// *nix headers
#ifndef _WIN32
#include <unistd.h>
#endif

// cURL headers
#include <curl/curl.h>

// Local headers
#include "jsonInterface.h"

//==========================================================================
// Class:			JSONInterface
// Function:		JSONInterface
//
// Description:		Constructor for JSONInterface class.
//
// Input Arguments:
//		userAgent	= const UString::String&
//
// Output Arguments:
//		None
//
// Return Value:
//		None
//
//==========================================================================
JSONInterface::JSONInterface(const UString::String& userAgent) : userAgent(userAgent)
{
}

//==========================================================================
// Class:			JSONInterface
// Function:		DoCURLPost
//
// Description:		Creates a cURL object, POSTs, obtains response, and cleans up.
//
// Input Arguments:
//		url					= const UString::String&
//		data				= const std::string&
//		curlModification	= CURLModification
//		modificationData	= const ModificationData*
//
// Output Arguments:
//		response	= std::string&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool JSONInterface::DoCURLPost(const UString::String &url, const std::string &data,
	std::string &response, CURLModification curlModification,
	const ModificationData* modificationData) const
{
	CURL *curl = curl_easy_init();
	if (!curl)
	{
		Cerr << "Failed to initialize CURL" << std::endl;
		return false;
	}

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, JSONInterface::CURLWriteCallback);
	response.clear();
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

	if (!caCertificatePath.empty())
		curl_easy_setopt(curl, CURLOPT_CAPATH, UString::ToNarrowString(caCertificatePath).c_str());

	if (verbose)
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

	if (!userAgent.empty())
		curl_easy_setopt(curl, CURLOPT_USERAGENT, UString::ToNarrowString(userAgent).c_str());

	curl_easy_setopt(curl, CURLOPT_POST, true);
/*	char *urlEncodedData = curl_easy_escape(curl, data.c_str(), data.length());
	if (!urlEncodedData)
	{
		Cerr << "Failed to url-encode the data" << std::endl;
		curl_easy_cleanup(curl);
		return false;
	}
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, urlEncodedData);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(urlEncodedData));*/

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.length());

	if (!curlModification(curl, modificationData))
		return false;

	curl_easy_setopt(curl, CURLOPT_URL, UString::ToNarrowString(url).c_str());
	CURLcode result = curl_easy_perform(curl);

//	curl_free(urlEncodedData);
	if(result != CURLE_OK)
	{
		Cerr << "Failed issuing https POST:  " << curl_easy_strerror(result) << "." << std::endl;
		curl_easy_cleanup(curl);
		return false;
	}

	curl_easy_cleanup(curl);
	return true;
}

//==========================================================================
// Class:			JSONInterface
// Function:		DoCURLGet
//
// Description:		Creates a cURL object, GETs, obtains response, and cleans up.
//
// Input Arguments:
//		url					= const std::string&
//		curlModification	= CURLModification
//		modificationData	= const ModificationData*
//
// Output Arguments:
//		response	= UString::String&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool JSONInterface::DoCURLGet(const UString::String &url, std::string &response,
	CURLModification curlModification, const ModificationData* modificationData) const
{
	CURL *curl = curl_easy_init();
	if (!curl)
	{
		Cerr << "Failed to initialize CURL\n";
		return false;
	}

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, JSONInterface::CURLWriteCallback);
	response.clear();
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

	if (!caCertificatePath.empty())
		curl_easy_setopt(curl, CURLOPT_CAPATH, UString::ToNarrowString(caCertificatePath).c_str());
	else
	{
		const auto curlCAFileEnvironmentVariable(std::getenv("CURL_CA_BUNDLE"));
		if (curlCAFileEnvironmentVariable)
			curl_easy_setopt(curl, CURLOPT_CAINFO, curlCAFileEnvironmentVariable);
	}

	if (!userAgent.empty())
		curl_easy_setopt(curl, CURLOPT_USERAGENT, UString::ToNarrowString(userAgent).c_str());

	if (verbose)
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

	if (!curlModification(curl, modificationData))
		return false;

	curl_easy_setopt(curl, CURLOPT_URL, UString::ToNarrowString(url).c_str());
	CURLcode result = curl_easy_perform(curl);

	if(result != CURLE_OK)
	{
		Cerr << "Failed issuing HTTP(S) GET:  " << curl_easy_strerror(result) << ".\n";
		curl_easy_cleanup(curl);
		return false;
	}

	curl_easy_cleanup(curl);
	return true;
}

//==========================================================================
// Class:			JSONInterface
// Function:		CURLWriteCallback
//
// Description:		Static member function for receiving returned data from cURL.
//
// Input Arguments:
//		ptr			= char*
//		size		= size_t indicating number of elements of size nmemb
//		nmemb		= size_t indicating size of each element
//		userData	= void* (must be pointer to UString::)
//
// Output Arguments:
//		None
//
// Return Value:
//		size_t indicating number of bytes read
//
//==========================================================================
size_t JSONInterface::CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData)
{
	size_t totalSize = size * nmemb;
//	((std::string*)userData)->clear();
	((std::string*)userData)->append(ptr, totalSize);

	return totalSize;
}

//==========================================================================
// Class:			JSONInterface
// Function:		ReadJSON
//
// Description:		Reads the specified field from the JSON array.
//
// Input Arguments:
//		root	= cJSON*
//		field	= const UString::String&
//
// Output Arguments:
//		value	= int&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool JSONInterface::ReadJSON(cJSON *root, const UString::String& field, int &value)
{
	cJSON *element = cJSON_GetObjectItem(root, UString::ToNarrowString(field).c_str());
	if (!element)
	{
		//Cerr << "Failed to read field '" << field << "' from JSON array\n";
		return false;
	}

	value = element->valueint;

	return true;
}

//==========================================================================
// Class:			JSONInterface
// Function:		ReadJSON
//
// Description:		Reads the specified field from the JSON array.
//
// Input Arguments:
//		root	= cJSON*
//		field	= const UString::String&
//
// Output Arguments:
//		value	= unsigned int&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool JSONInterface::ReadJSON(cJSON *root, const UString::String& field, unsigned int &value)
{
	cJSON *element = cJSON_GetObjectItem(root, UString::ToNarrowString(field).c_str());
	if (!element)
	{
		//Cerr << "Failed to read field '" << field << "' from JSON array\n";
		return false;
	}

	value = static_cast<unsigned int>(element->valueint);

	return true;
}

//==========================================================================
// Class:			JSONInterface
// Function:		ReadJSON
//
// Description:		Reads the specified field from the JSON array.
//
// Input Arguments:
//		root	= cJSON*
//		field	= const UString::String&
//
// Output Arguments:
//		value	= UString::String&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool JSONInterface::ReadJSON(cJSON *root, const UString::String& field, UString::String &value)
{
	cJSON *element = cJSON_GetObjectItem(root, UString::ToNarrowString(field).c_str());
	if (!element)
	{
		//Cerr << "Failed to read field '" << field << "' from JSON array\n";
		return false;
	}

	if (element->valuestring)
		value = UString::ToStringType(element->valuestring);

	return true;
}

//==========================================================================
// Class:			JSONInterface
// Function:		ReadJSON
//
// Description:		Reads the specified field from the JSON array.
//
// Input Arguments:
//		root	= cJSON*
//		field	= const UString::String&
//
// Output Arguments:
//		value	= double&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool JSONInterface::ReadJSON(cJSON *root, const UString::String& field, double &value)
{
	cJSON *element = cJSON_GetObjectItem(root, UString::ToNarrowString(field).c_str());
	if (!element)
	{
		//Cerr << "Failed to read field '" << field << "' from JSON array\n";
		return false;
	}

	value = element->valuedouble;

	return true;
}

//==========================================================================
// Class:			JSONInterface
// Function:		ReadJSON
//
// Description:		Reads the specified field from the JSON array.
//
// Input Arguments:
//		root	= cJSON*
//		field	= const UString::String&
//
// Output Arguments:
//		value	= std::chrono::steady_clock::time_point&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool JSONInterface::ReadJSON(cJSON *root, const UString::String& field, std::tm &value)
{
	cJSON *element = cJSON_GetObjectItem(root, UString::ToNarrowString(field).c_str());
	if (!element)
	{
		//Cerr << "Failed to read field '" << field << "' from JSON array\n";
		return false;
	}

	UString::IStringStream ss(UString::ToStringType(element->valuestring));
	// Possible bug in MSVC's implemention of std::get_time (in 2015 and 2017)
	// Doesn't like format UString::s longer than actual UString::
	const UString::String format(_T("%Y-%m-%d %H:%M"));
	const size_t formatLength(11);// Minimum possible length of input UString::String to successfully fill out this format
	if (ss.str().length() < formatLength)
		return false;
	if ((ss >> std::get_time(&value, format.c_str())).fail())
	{
		//Cerr << "Failed to parse data for field '" << field << "'\n";
		return false;
	}

	return true;
}

//==========================================================================
// Class:			JSONInterface
// Function:		ReadJSON
//
// Description:		Reads the specified field from the JSON array.
//
// Input Arguments:
//		root	= cJSON*
//		field	= const UString::String&
//
// Output Arguments:
//		value	= bool&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool JSONInterface::ReadJSON(cJSON *root, const UString::String& field, bool &value)
{
	cJSON *element = cJSON_GetObjectItem(root, UString::ToNarrowString(field).c_str());
	if (!element)
	{
		//Cerr << "Failed to read field '" << field << "' from JSON array\n";
		return false;
	}

	value = element->valueint == 1;

	return true;
}

//==========================================================================
// Class:			JSONInterface
// Function:		URLEncode
//
// Description:		Encodes special characters as required to conform to the W3
//					Uniform Resource Identifier specification.
//
// Input Arguments:
//		root	= cJSON*
//		field	= const UString::String&
//
// Output Arguments:
//		value	= double&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
UString::String JSONInterface::URLEncode(const UString::String& s)
{
	UString::String encoded;
	for (const auto& c : s)
	{
		if (c == UString::Char(' '))
			encoded.append(_T("%20"));
		else if (c == UString::Char('"'))
			encoded.append(_T("%22"));
		else if (c == UString::Char('<'))
			encoded.append(_T("%3C"));
		else if (c == UString::Char('>'))
			encoded.append(_T("%3E"));
		else if (c == UString::Char('#'))
			encoded.append(_T("%23"));
		else if (c == UString::Char('%'))
			encoded.append(_T("%25"));
		else if (c == UString::Char('|'))
			encoded.append(_T("%7C"));
		else
			encoded.append(UString::String(&c, 1));
	}

	return encoded;
}

//==========================================================================
// Class:			JSONInterface
// Function:		ReadJSONArrayToVector
//
// Description:		Reads JSON array into vector using each array entry as
//					a vector element.
//
// Input Arguments:
//		root	= cJSON*
//		field	= const UString::String&
//
// Output Arguments:
//		v		= std::vector<UString::String>&
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool JSONInterface::ReadJSONArrayToVector(cJSON *root, const UString::String& field, std::vector<UString::String>& v)
{
	cJSON* arrayParent(cJSON_GetObjectItem(root, UString::ToNarrowString(field).c_str()));
	if (!arrayParent)
		return false;

	v.resize(cJSON_GetArraySize(arrayParent));
	unsigned int i(0);
	for (auto& item : v)
	{
		cJSON* arrayItem(cJSON_GetArrayItem(arrayParent, i));
		if (!arrayItem)
			return false;

		item = UString::ToStringType(arrayItem->valuestring);
		++i;
	}

	return true;
}
