// File:  jsonInterface.h
// Date:  7/7/2016
// Auth:  K. Loux
// Desc:  Class for interfacing with a remote server using JSON and CURL.

#ifndef JSON_INTERFACE_H_
#define JSON_INTERFACE_H_

// Local headers
#include "cJSON/cJSON.h"

// Standard C++ headers
#include <string>
#include <vector>
#include <ctime>

// cJSON forward declarations
struct cJSON;

// for cURL
typedef void CURL;

class JSONInterface
{
public:
	JSONInterface(const std::string& userAgent = "");
	virtual ~JSONInterface() = default;

	void SetCACertificatePath(const std::string& path) { caCertificatePath = path; }
	void SetVerboseOutput(const bool& verboseOutput = true) { verbose = verboseOutput; }

private:
	const std::string userAgent;
	static bool DoNothing(CURL*) { return true; }

protected:
	std::string caCertificatePath;
	bool verbose = false;

	typedef bool (*CURLModification)(CURL*);

	bool DoCURLPost(const std::string &url, const std::string &data,
		std::string &response, CURLModification curlModification = &JSONInterface::DoNothing) const;
	bool DoCURLGet(const std::string &url, std::string &response,
		CURLModification curlModification  = &JSONInterface::DoNothing) const;

	static bool ReadJSON(cJSON* root, const std::string& field, int& value);
	static bool ReadJSON(cJSON* root, const std::string& field, unsigned int& value);
	static bool ReadJSON(cJSON* root, const std::string& field, std::string &value);
	static bool ReadJSON(cJSON* root, const std::string& field, double& value);
	static bool ReadJSON(cJSON* root, const std::string& field, std::tm& value);
	static bool ReadJSON(cJSON* root, const std::string& field, bool& value);

	template <typename T>
	static bool ReadJSON(cJSON *root, const std::string& field, std::vector<T>& v);

	static bool ReadJSONArrayToVector(cJSON *root, const std::string& field, std::vector<std::string>& v);

	static size_t CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData);

	static std::string URLEncode(const std::string& s);
};

template <typename T>
bool JSONInterface::ReadJSON(cJSON *root, const std::string& field, std::vector<T>& v)
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
