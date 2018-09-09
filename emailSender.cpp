// File:  emailSender.cpp
// Date:  4/10/2013
// Auth:  K. Loux
// Desc:  Class which uses libcurl to send e-mails.

// Standard C++ headers
#include <iostream>
#include <fstream>
#include <time.h>
#include <sstream>
#include <cstring>

// OS headers
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif// NOMINMAX
#include <Windows.h>
#else
#include <sys/time.h>
#endif

// Standard C++ headers
#include <cassert>
#include <algorithm>

// cURL headers
#include <curl/curl.h>

// Local headers
#include "emailSender.h"
#include "oAuth2Interface.h"

// rpi headers
#include "utilities/timingUtility.h"

//==========================================================================
// Class:			EmailSender
// Function:		EmailSender
//
// Description:		Constructor for EmailSender class.
//
// Input Arguments:
//		subject			= const std::string&
//		message			= const std::string&
//		imageFileName	= const std::string&
//		recipients		= const std::vector<AddressInfo>&
//		loginInfo		= const SystemEmailConfiguration
//		useHTML			= const bool&
//		testMode		= const bool&
//
// Output Arguments:
//		None
//
// Return Value:
//		None
//
//==========================================================================
EmailSender::EmailSender(const std::string &subject, const std::string &message,
	const std::string &imageFileName, const std::vector<AddressInfo> &recipients,
	const LoginInfo &loginInfo, const bool &useHTML, const bool& testMode,
	UString::OStream &outStream) : subject(subject), message(message), imageFileName(imageFileName),
	recipients(recipients), loginInfo(loginInfo), useHTML(useHTML), testMode(testMode),
	outStream(outStream)
{
	assert(recipients.size() > 0);
	assert(!useHTML || imageFileName.empty());

	if (testMode)
	{
		outStream << "Using cURL version:" << std::endl << curl_version() << std::endl;
		outStream << "Image file name: '" << UString::ToStringType(imageFileName) << "'." << std::endl;
	}
}

//==========================================================================
// Class:			EmailSender
// Function:		Send
//
// Description:		Sends e-mail as specified.
//					http://talkbinary.com/programming/c/how-to-send-email-through-gmail-using-libcurl/
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
bool EmailSender::Send()
{
	CURL *curl = curl_easy_init();
	CURLcode result;

	if (!curl)
		return false;

	GeneratePayloadText();
	uploadCtx.linesRead = 0;
	uploadCtx.et = this;

	struct curl_slist *recipientList = nullptr;

	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl, CURLOPT_URL, loginInfo.smtpUrl.c_str());

	if (!loginInfo.caCertificatePath.empty())
		curl_easy_setopt(curl, CURLOPT_CAPATH, loginInfo.caCertificatePath.c_str());

	if (loginInfo.oAuth2Token.empty())
	{
		if (loginInfo.useSSL)
			curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
		curl_easy_setopt(curl, CURLOPT_PASSWORD, loginInfo.password.c_str());
	}
	else
	{
		curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
		curl_easy_setopt(curl, CURLOPT_XOAUTH2_BEARER, UString::ToNarrowString(OAuth2Interface::Get().GetAccessToken()).c_str());
	}

	if (testMode)
	{
		outStream << "Sending messages from " << UString::ToStringType(loginInfo.localEmail) << " to ";
		unsigned int i;
		for (i = 0; i < recipients.size(); i++)
		{
			if (i > 0)
				outStream << ", ";
			outStream << UString::ToStringType(recipients[i].address);
		}
		outStream << std::endl;
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	}

	curl_easy_setopt(curl, CURLOPT_USERNAME, loginInfo.localEmail.c_str());
	curl_easy_setopt(curl, CURLOPT_MAIL_FROM, ("<" + loginInfo.localEmail + ">").c_str());

	for (const auto& r : recipients)
		recipientList = curl_slist_append(recipientList, ("<" + r.address + ">").c_str());
	curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipientList);

	curl_easy_setopt(curl, CURLOPT_READFUNCTION, &EmailSender::PayloadSource);
	curl_easy_setopt(curl, CURLOPT_READDATA, &uploadCtx);

    result = curl_easy_perform(curl);
    if(result != CURLE_OK)
		outStream << "Failed sending e-mail:  " << curl_easy_strerror(result) << std::endl;

    curl_slist_free_all(recipientList);
    curl_easy_cleanup(curl);

	return result == CURLE_OK;
}

//==========================================================================
// Class:			EmailSender
// Function:		GeneratePayloadText
//
// Description:		Generates e-mail payload.
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
void EmailSender::GeneratePayloadText()
{
	payloadText.clear();
	GenerateMessageText();

	unsigned int payloadLines(7 + messageText.size());
	std::string base64File;
	if (!imageFileName.empty())
	{
		assert(!useHTML);
		unsigned int lines;
		base64File = Base64Encode(imageFileName, lines);
		payloadLines += 18 + lines;// Not sure why 18 (I count 16), but there is an extra +2 that needs to be here
	}
	else if (useHTML)
		payloadLines += 11;

	payloadText.resize(payloadLines);

	std::string list;
	unsigned int k(0);
	for (const auto& r : recipients)
	{
		if (!list.empty())
			list.append(", ");
		list.append(NameToHeaderAddress(r));
	}

	std::string boundary(GenerateBoundryID());

	// Normal header
	payloadText[k] = "Date: " + GetDateString() + "\n"; k++;
	payloadText[k] = "To: " + list + "\n"; k++;
	payloadText[k] = "From: " + loginInfo.localEmail + "\n"; k++;
	payloadText[k] = "Message-ID: " + GenerateMessageID() + "\n"; k++;
	payloadText[k] = "Subject: " + subject + "\n"; k++;

	// Special header contents when attaching a file
	if (!imageFileName.empty())
	{
		payloadText[k] = "Content-Type: multipart/mixed; boundary=" + boundary + "\n"; k++;
		payloadText[k] = "MIME-Version: 1.0\n"; k++;
		payloadText[k] = "\n"; k++;
		payloadText[k] = "This is a multi-part message in MIME format.\n"; k++;
		payloadText[k] = "\n"; k++;
		payloadText[k] = "--" + boundary + "\n"; k++;
		payloadText[k] = "Content-Type: text/plain; charset=ISO-8859-1\n"; k++;
		payloadText[k] = "Content-Transfer-Encoding: quoted-printable\n"; k++;
	}
	else if (useHTML)
	{
		payloadText[k] = "Content-Type: text/html; charset=ISO-8859-1\n"; k++;
		payloadText[k] = "Content-Transfer-Encoding: quoted-printable\n"; k++;
		payloadText[k] = "MIME-Version: 1.0\n"; k++;
		payloadText[k] = "\n"; k++;
		payloadText[k] = "<html>\n"; k++;
		payloadText[k] = "<head>\n"; k++;
		payloadText[k] = "<meta http-equiv=3D\"Content-Type\" content=3D\"text/html; charset=3D\"ISO-8859-1\">\n"; k++;
		payloadText[k] = "</head>\n"; k++;
		payloadText[k] = "<body>\n"; k++;
	}

	// Normal body
	payloadText[k] = "\n"; k++;// Empty line to divide headers from body
	for (const auto& messageLine : messageText)
	{
		payloadText[k] = messageLine;
		k++;
	}

	// Special body contents when attaching a file
	if (!imageFileName.empty())
	{
		std::string image = imageFileName.substr(imageFileName.find_last_of('/') + 1);

		payloadText[k] = "\n"; k++;
		payloadText[k] = "--" + boundary + "\n"; k++;
		payloadText[k] = "Content-Type: image/" + GetExtension(imageFileName) + ";\n"; k++;
		payloadText[k] = "	name=\"" + image + "\"\n"; k++;
		payloadText[k] = "Content-Transfer-Encoding: base64\n"; k++;
		payloadText[k] = "Content-Disposition: attachment;\n"; k++;
		payloadText[k] = "	filename=\"" + image + "\";\n"; k++;

		size_t cr(0);
		while (cr != std::string::npos)
		{
			const size_t lastCr(cr);
			cr = base64File.find("\n", lastCr + 1);
			payloadText[k] = base64File.substr(lastCr,
				std::min(cr - lastCr, (size_t)base64File.size() - lastCr)); k++;
		}
		payloadText[k] = "--" + boundary + "\n"; k++;
	}
	else if (useHTML)
	{
		// TODO:  Should it be the caller's responsiblity to already have
		// formatted the message to include these tags?
		payloadText[k] = "</body>\n"; k++;
		payloadText[k] = "</html>\n"; k++;
	}

	payloadText[k] = "\0"; k++;

	assert(k == payloadLines);
}

//==========================================================================
// Class:			EmailSender
// Function:		GenerateMessageText
//
// Description:		Creates the buffer for the message text.
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
void EmailSender::GenerateMessageText()
{
	messageText.clear();
	std::istringstream mStream(message);
	std::string line;

	while (std::getline(mStream, line))
		messageText.push_back(line + '\n');
}

//==========================================================================
// Class:			EmailSender
// Function:		NameToHeaderAddress
//
// Description:		Converts from a name to an address and name (i.e.
//					user@domain (Name)).
//
// Input Arguments:
//		a		= const AddressInfo
//
// Output Arguments:
//		None
//
// Return Value:
//		std::string
//
//==========================================================================
std::string EmailSender::NameToHeaderAddress(const AddressInfo& a)
{
	return a.displayName + " (" + a.address + ")";
}

//==========================================================================
// Class:			EmailSender
// Function:		GetDateString
//
// Description:		Returns the formatted date string for the e-mail header.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		std::string
//
//==========================================================================
std::string EmailSender::GetDateString()
{
	time_t nowTime;
	time(&nowTime);

	const unsigned int bufferSize(80);
	char buffer[bufferSize];

#ifdef _WIN32
	struct tm now;
	localtime_s(&now, &nowTime);
	strftime(buffer, bufferSize, "%a, %d %b %Y %H:%M:%S %z", &now);
#else
	struct tm *now;
	now = localtime(&nowTime);
	strftime(buffer, bufferSize, "%a, %d %b %Y %H:%M:%S %z", now);
#endif

	return buffer;
}

//==========================================================================
// Class:			EmailSender
// Function:		GenerateMessageID
//
// Description:		Generates a unique message ID.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		std::string
//
//==========================================================================
std::string EmailSender::GenerateMessageID() const
{
	UString::String id(_T("<"));
	id.append(OAuth2Interface::Base36Encode(
		std::chrono::duration_cast<std::chrono::milliseconds>(
		TimingUtility::Clock::now().time_since_epoch()).count()));
	id.append(_T("."));
	id.append(OAuth2Interface::Base36Encode((int64_t)rand() * (int64_t)rand()
		* (int64_t)rand() * (int64_t)rand()));
	id.append(_T("@"));
	id.append(UString::ToStringType(ExtractDomain(loginInfo.localEmail)));
	id.append(_T(">"));

	return UString::ToNarrowString(id);
}

//==========================================================================
// Class:			EmailSender
// Function:		GenerateBoundryID
//
// Description:		Generates a unique boundary ID.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		std::string
//
//==========================================================================
std::string EmailSender::GenerateBoundryID()
{
	return UString::ToNarrowString(OAuth2Interface::Base36Encode(
		(int64_t)rand() * (int64_t)rand()
		* (int64_t)rand() * (int64_t)rand()
		* (int64_t)rand() * (int64_t)rand()
		* (int64_t)rand() * (int64_t)rand()));
}

//==========================================================================
// Class:			EmailSender
// Function:		PayloadSource
//
// Description:		Copies message payload to the send buffer.
//
// Input Arguments:
//		ptr		= void*
//		size	= size_t
//		nmemb	= size_t
//		userp	= void*
//
// Output Arguments:
//		None
//
// Return Value:
//		size_t
//
//==========================================================================
size_t EmailSender::PayloadSource(void *ptr, size_t size, size_t nmemb, void *userp)
{
	struct UploadStatus *uploadCtx = static_cast<struct UploadStatus*>(userp);
	const char *data;

	if ((size == 0) || (nmemb == 0) || ((size * nmemb) < 1))
		return 0;

	data = uploadCtx->et->payloadText[uploadCtx->linesRead].c_str();
	if (data)
	{
		size_t len = strlen(data);
		memcpy(ptr, data, len);
		uploadCtx->linesRead++;
		return len;
	}

	return 0;
}

//==========================================================================
// Class:			EmailSender
// Function:		ExtractDomain
//
// Description:		Extracts the domain from an e-mail address.
//
// Input Arguments:
//		s	= const std::string&
//
// Output Arguments:
//		None
//
// Return Value:
//		std::string
//
//==========================================================================
std::string EmailSender::ExtractDomain(const std::string &s)
{
	size_t start = s.find("@");
	if (start == std::string::npos)
		return "";

	return s.substr(start + 1);
}

//==========================================================================
// Class:			EmailSender
// Function:		Base64Encode
//
// Description:		Encodes the specified file into a base64 string.
//
// Input Arguments:
//		fileName	= const std::string
//
// Output Arguments:
//		lines	= unsigned int&
//
// Return Value:
//		std::string
//
//==========================================================================
std::string EmailSender::Base64Encode(const std::string &fileName, unsigned int &lines)
{
	lines = 1;
	std::ifstream inFile(fileName.c_str(), std::ios::binary);
	if (!inFile.is_open() || !inFile.good())
		return std::string();

	// FIXME:  Perofrmance could be improved by pre-allocating the return buffer

	inFile >> std::noskipws;
	unsigned char uc;
	std::vector<unsigned char> v;
	while (inFile >> uc, !inFile.eof())
		v.push_back(uc);

	const char* charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	unsigned int i;
	lines = 0;
	std::string buf;
	for (i = 0; i < v.size(); i += 3)
	{
		unsigned char oct1(v[i]);
		unsigned char oct2(0);
		unsigned char oct3(0);

		if (i + 1 < v.size())
			oct2 = v[i + 1];
		if (i + 2 < v.size())
			oct3 = v[i + 2];

		unsigned char hex1(oct1 >> 2);
		unsigned char hex2(((oct1 & 0x3) << 4) | (oct2 >> 4));
		unsigned char hex3(((oct2 & 0xf) << 2) | (oct3 >> 6));
		unsigned char hex4(oct3 & 0x3f);

		buf += charset[hex1];
		buf += charset[hex2];

		if (i + 1 < v.size())
			buf += charset[hex3];
		else
			buf += "=";

		if (i + 2 < v.size())
			buf += charset[hex4];
		else
			buf += "=";

		if ((buf.size() - lines) % 76 == 0)
		{
			buf += "\n";
			lines++;
		}
	}

	buf += "\n";

	return buf;
}

//==========================================================================
// Class:			EmailSender
// Function:		GetExtension
//
// Description:		Returns the extension (everything after the '.') for the
//					specified text.
//
// Input Arguments:
//		s	= const std::string&
//
// Output Arguments:
//		None
//
// Return Value:
//		std::string
//
//==========================================================================
std::string EmailSender::GetExtension(const std::string &s)
{
	return s.substr(s.find_last_of('.') + 1);
}
