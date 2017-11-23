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

// cJSON forward declarations
struct cJSON;

class JSONInterface
{
public:
	JSONInterface(const std::string& userAgent = "");
	virtual ~JSONInterface() = default;

	void SetCACertificatePath(const std::string& path) { caCertificatePath = path; }
	void SetVerboseOutput(const bool& verboseOutput = true) { verbose = verboseOutput; }

private:
	const std::string userAgent;

protected:
	std::string caCertificatePath;
	bool verbose = false;

	bool DoCURLPost(const std::string &url, const std::string &data,
		std::string &response) const;
	bool DoCURLGet(const std::string &url, std::string &response) const;

	static bool ReadJSON(cJSON *root, const std::string& field, int &value);
	static bool ReadJSON(cJSON *root, const std::string& field, unsigned int &value);
	static bool ReadJSON(cJSON *root, const std::string& field, std::string &value);
	static bool ReadJSON(cJSON *root, const std::string& field, double &value);

	template <typename T>
	static bool ReadJSON(cJSON *root, const std::string& field, std::vector<T>& v);

	static size_t CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData);

	static std::string URLEncode(const std::string& s);
};

template <typename T>
bool JSONInterface::ReadJSON(cJSON *root, const std::string& field, std::vector<T>& v)
{
	const unsigned int arraySize(cJSON_GetArraySize(root));
	unsigned int i;
	for (i = 0; i < arraySize; ++i)
	{
		cJSON* arrayItem(cJSON_GetArrayItem(root, i));

		T value;
		if (!ReadJSON(arrayItem, field, value))
			return false;

		v.push_back(value);
	}

	return true;
}

#endif// JSON_INTERFACE_H_
