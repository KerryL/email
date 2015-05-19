// File:  emailSender.h
// Date:  4/10/2013
// Auth:  K. Loux
// Desc:  Class which uses libcurl to send e-mails.

#ifndef _EMAIL_SENDER_H_
#define _EMAIL_SENDER_H_

// Standard C++ headers
#include <vector>
#include <string>

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
	};

	EmailSender(const std::string &subject, const std::string &message, const std::string &imageFileName,
		const std::vector<std::string> &recipients, const LoginInfo &loginInfo,
		const bool &testMode, std::ostream &outStream = std::cout);
	virtual ~EmailSender();

	bool Send();

private:
	const std::string subject;
	const std::string message;
	const std::string imageFileName;
	const std::vector<std::string> &recipients;
	const LoginInfo loginInfo;
	const bool testMode;
	std::ostream &outStream;

	struct UploadStatus
	{
		int linesRead;
		EmailSender* et;
	} uploadCtx;

	static size_t PayloadSource(void *ptr, size_t size, size_t nmemb, void *userp);
	void GeneratePayloadText(void);
	char* AddPayloadText(const std::string &s) const;
	char **payloadText;
	unsigned int payloadLines;
	void DeletePayloadText(void);

	void GenerateMessageText(void);
	char **messageText;
	unsigned int messageLines;
	void DeleteMessageText(void);

	std::string NameToHeaderAddress(const std::string &s);
	static std::string GetDateString(void);
	std::string GenerateMessageID(void) const;
	static std::string GenerateBoundryID(void);
	static std::string ExtractDomain(const std::string &s);
	static std::string Base64Encode(const std::string &fileName, unsigned int &lines);
	static long long GetMillisecondsSinceEpoch(void);
	static std::string GetExtension(const std::string &s);
};

#endif// _EMAIL_SENDER_H_
