/*===================================================================================
                                        Watcher
                             Copyright Kerry R. Loux 2013

               Uses:
	               wxWidgets:  http://www.wxwidgets.org/
	               Live555:    http://www.live555.com/
		           FFMPEG:     http://www.ffmpeg.org/
				   cURL:       http://curl.haxx.se/libcurl/

===================================================================================*/

// File:  emailSender.h
// Date:  4/10/2013
// Auth:  K. Loux
// Desc:  wxThread derived class which uses libcurl to send e-mails.

#ifndef _EMAIL_SENDER_H_
#define _EMAIL_SENDER_H_

// Standard C++ headers
#include <vector>
#include <string>

// wxWidgets headers
#include <wx/wx.h>

// Local headers
#include "configInfo.h"

class EmailSender
{
public:
	EmailSender(const std::string &subject, const std::string &message, const std::string &imageFileName,
		const std::vector<std::string> &recipients, const LoginInfo &systemConfig,
		const bool &testMode);
	virtual ~EmailSender();

	bool Send();

private:
	const std::string subject;
	const std::string message;
	const std::string imageFileName;
	const std::vector<std::string> &recipients;
	const LoginInfo systemConfig;
	const bool testMode;

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
