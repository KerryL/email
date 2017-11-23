// File:  curlUtilities.h
// Date:  11/22/2017
// Auth:  K. Loux
// Desc:  Utilities for working with libCURL.

#ifndef CURL_UTILITIES_H_
#define CURL_UTILITIES_H_

// libCURL headers
#include <curl/curl.h>

namespace CURLUtilities
{
	bool CURLCallHasError(const CURLcode& result, const std::string& message);
}

#endif// CURL_UTILITIES_H_
