// File:  curlUtilities.cpp
// Date:  11/22/2017
// Auth:  K. Loux
// Desc:  Utilities for working with libCURL.

// Local headers
#include "curlUtilities.h"

// Standard C++ headers
#include <iostream>

//==========================================================================
// Class:			CURLUtilities
// Function:		CURLCallHasError
//
// Description:		Wrapper for checking return codes on CURL calls.
//
// Input Arguments:
//		result	= const CURLcode&
//		message	= const UString::String&
//
// Output Arguments:
//		None
//
// Return Value:
//		bool, true for success, false otherwise
//
//==========================================================================
bool CURLUtilities::CURLCallHasError(const CURLcode& result, const UString::String& message)
{
	if (result == CURLE_OK)
		return false;

	Cerr << message << ":  " << curl_easy_strerror(result) << '\n';
	return true;
}

