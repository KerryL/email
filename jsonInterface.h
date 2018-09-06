// File:  jsonInterface.h
// Date:  7/7/2016
// Auth:  K. Loux
// Desc:  Class for interfacing with a remote server using JSON and CURL.

#ifndef JSON_INTERFACE_H_
#define JSON_INTERFACE_H_

// Local headers
#include "cJSON/cJSON.h"

// utilities headers
#include "utilities/uString.h"

// Standard C++ headers
#include <vector>
#include <ctime>

// cJSON forward declarations
struct cJSON;

// for cURL
typedef void CURL;

class JSONInterface
{
public:
	explicit JSONInterface(const UString::String& userAgent = UString::String());
	virtual ~JSONInterface() = default;

	void SetCACertificatePath(const UString::String& path) { caCertificatePath = path; }
	void SetVerboseOutput(const bool& verboseOutput = true) { verbose = verboseOutput; }

private:
	const UString::String userAgent;

protected:
	UString::String caCertificatePath;
	bool verbose = false;

	struct ModificationData
	{
	};

	typedef bool (*CURLModification)(CURL*, const ModificationData*);
	static bool DoNothing(CURL*, const ModificationData*) { return true; }

	bool DoCURLPost(const UString::String &url, const std::string &data,
		std::string &response, CURLModification curlModification = &JSONInterface::DoNothing,
		const ModificationData* modificationData = nullptr) const;
	bool DoCURLGet(const UString::String &url, std::string &response,
		CURLModification curlModification  = &JSONInterface::DoNothing,
		const ModificationData* modificationData = nullptr) const;

	static bool ReadJSON(cJSON* root, const UString::String& field, int& value);
	static bool ReadJSON(cJSON* root, const UString::String& field, unsigned int& value);
	static bool ReadJSON(cJSON* root, const UString::String& field, UString::String &value);
	static bool ReadJSON(cJSON* root, const UString::String& field, double& value);
	static bool ReadJSON(cJSON* root, const UString::String& field, std::tm& value);
	static bool ReadJSON(cJSON* root, const UString::String& field, bool& value);

	template <typename T>
	static bool ReadJSON(cJSON *root, const UString::String& field, std::vector<T>& v);

	static bool ReadJSONArrayToVector(cJSON *root, const UString::String& field, std::vector<UString::String>& v);

	static size_t CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData);

	static UString::String URLEncode(const UString::String& s);
};

template <typename T>
bool JSONInterface::ReadJSON(cJSON *root, const UString::String& field, std::vector<T>& v)
{
	v.resize(cJSON_GetArraySize(root));
	unsigned int i(0);
	for (auto& item : v)
	{
		cJSON* arrayItem(cJSON_GetArrayItem(root, i));
		if (!arrayItem)
			return false;

		if (!ReadJSON(arrayItem, field, item))
			return false;

		++i;
	}

	return true;
}

#endif// JSON_INTERFACE_H_
