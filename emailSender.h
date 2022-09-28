// File:  emailSender.h
// Date:  4/10/2013
// Auth:  K. Loux
// Desc:  Class which uses libcurl to send e-mails.

#ifndef EMAIL_SENDER_H_
#define EMAIL_SENDER_H_

// Local headers
#include "utilities/uString.h"
#include "jsonInterface.h"

// cURL headers
#include <curl/curl.h>

// Standard C++ headers
#include <string>
#include <vector>
#include <memory>

class EmailSender
{
public:
	struct LoginInfo
	{
		std::string smtpUrl;
		std::string localEmail;
		std::string oAuth2Token;
		std::string password;
		bool useSSL;
		std::string caCertificatePath;
	};

	struct AddressInfo
	{
		std::string address;
		std::string displayName;
	};

	EmailSender(const std::string &subject, const std::string &message, const std::string &attachmentFileName,
		const std::vector<AddressInfo> &recipients, const LoginInfo &loginInfo, const bool &useHTML,
		const bool &testMode, UString::OStream &outStream = Cout);

	bool Send();

	bool SendREST();

	void DisableSignaling(const bool& disable = true) { disableSignaling = disable; }

private:
	const std::string subject;
	const std::string message;
	const std::string attachmentFileName;
	const std::vector<AddressInfo> recipients;
	const LoginInfo loginInfo;
	const bool useHTML;
	const bool testMode;
	bool disableSignaling;
	UString::OStream &outStream;

	struct UploadStatus
	{
		int linesRead = 0;
		EmailSender* et = nullptr;
	} uploadCtx;

	static size_t PayloadSource(void *ptr, size_t size, size_t nmemb, void *userp);
	void GeneratePayloadText();
	std::vector<std::string> payloadText;

	void GenerateMessageText();
	std::vector<std::string> messageText;

	std::string NameToHeaderAddress(const AddressInfo &a);
	static std::string GetDateString();
	std::string GenerateMessageID() const;
	static std::string GenerateBoundryID();
	static std::string ExtractDomain(const std::string &s);
	static std::string Base64EncodeFile(const std::string &fileName, unsigned int &lines);
	static std::string Base64Encode(const std::string &s, const bool& wrapLines = true, unsigned int *lines = nullptr);
	static std::string GetExtension(const std::string &s);
	
	static bool IsImageExtension(std::string extension);
	
	static int DebugCallback(CURL* handle, curl_infotype type, char* data, size_t size, void *userp);

	class EmailPOSTer : public JSONInterface
	{
	public:
		struct AdditionalPostData : public ModificationData
		{
			curl_slist* headerList = nullptr;
		};

		bool POST(const UString::String &url, const std::string &data, const AdditionalPostData& additionalData, std::string &response);

	private:
		static bool AddOAuthToken(CURL* curl, const ModificationData* tokenInfo);
	};
};

#endif// EMAIL_SENDER_H_
