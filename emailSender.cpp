// File:  emailSender.cpp
// Date:  4/10/2013
// Auth:  K. Loux
// Desc:  Class which uses libcurl to send e-mails.

// Standard C++ headers
#include <iostream>
#include <fstream>
#include <time.h>
#include <sstream>

// OS headers
#ifdef _WIN32
#define NOMINMAX
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
//		recipients		= const std::vector<std::string>&
//		loginInfo		= const SystemEmailConfiguration
//		testMode		= const TestConfiguration::TestMode&
//
// Output Arguments:
//		None
//
// Return Value:
//		None
//
//==========================================================================
EmailSender::EmailSender(const std::string &subject, const std::string &message,
	const std::string &imageFileName, const std::vector<std::string> &recipients,
	const LoginInfo &loginInfo, const bool& testMode, std::ostream &outStream) :
	subject(subject), message(message), imageFileName(imageFileName),
	recipients(recipients), loginInfo(loginInfo), testMode(testMode), outStream(outStream)
{
	assert(recipients.size() > 0);

	if (testMode)
	{
		outStream << "Using cURL version:" << std::endl << curl_version() << std::endl;
		outStream << "Image file name: '" << imageFileName << "'." << std::endl;
	}

	payloadLines = 0;
	payloadText = NULL;

	messageLines = 0;
	messageText = NULL;
}

//==========================================================================
// Class:			EmailSender
// Function:		~EmailSender
//
// Description:		Destructor for EmailSender class.
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
EmailSender::~EmailSender()
{
	DeletePayloadText();
	DeleteMessageText();
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

	struct curl_slist *recipientList = NULL;

	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl, CURLOPT_URL, loginInfo.smtpUrl.c_str());

	if (loginInfo.oAuth2Token.empty())
	{
		if (loginInfo.useSSL)
			curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
		curl_easy_setopt(curl, CURLOPT_PASSWORD, loginInfo.password.c_str());
	}
	else
	{
		curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
		curl_easy_setopt(curl, CURLOPT_XOAUTH2_BEARER, OAuth2Interface::Get().GetAccessToken().c_str());
	}

	if (testMode)
	{
		outStream << "Sending messages from " << loginInfo.localEmail << " to ";
		unsigned int i;
		for (i = 0; i < recipients.size(); i++)
		{
			if (i > 0)
				outStream << ", ";
			outStream << recipients[i];
		}
		outStream << std::endl;
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	}

	curl_easy_setopt(curl, CURLOPT_USERNAME, loginInfo.localEmail.c_str());
	curl_easy_setopt(curl, CURLOPT_MAIL_FROM, ("<" + loginInfo.localEmail + ">").c_str());

	unsigned int i;
	for (i = 0; i < recipients.size(); i++)
		recipientList = curl_slist_append(recipientList, ("<"
		+ recipients[i] + ">").c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipientList);

	curl_easy_setopt(curl, CURLOPT_READFUNCTION, &EmailSender::PayloadSource);
    curl_easy_setopt(curl, CURLOPT_READDATA, &uploadCtx);

    result = curl_easy_perform(curl);
    if(result != CURLE_OK)
		std::cerr << "Failed sending e-mail:  " << curl_easy_strerror(result) << std::endl;

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
void EmailSender::GeneratePayloadText(void)
{
	DeletePayloadText();
	GenerateMessageText();

	payloadLines = 7 + messageLines;
	std::string base64File;
	if (!imageFileName.empty())
	{
		unsigned int lines;
		base64File = Base64Encode(imageFileName, lines);
		payloadLines += 18 + lines;// Not sure why 18 (I count 16), but there is an extra +2 that needs to be here
	}
	payloadText = new char*[payloadLines];

	std::string list(NameToHeaderAddress(recipients[0]));
	unsigned int i, k(0);
	for (i = 1; i < recipients.size(); i++)
		list.append(", " + NameToHeaderAddress(recipients[i]));

	std::string boundary(GenerateBoundryID());

	// Normal header
	payloadText[k] = AddPayloadText("Date: " + GetDateString() + "\n"); k++;
	payloadText[k] = AddPayloadText("To: " + list + "\n"); k++;
	payloadText[k] = AddPayloadText("From: " + loginInfo.localEmail + "\n"); k++;
	payloadText[k] = AddPayloadText("Message-ID: " + GenerateMessageID() + "\n"); k++;
	payloadText[k] = AddPayloadText(("Subject: " + subject + "\n").c_str()); k++;

	// Special header contents when attaching a file
	if (!imageFileName.empty())
	{
		payloadText[k] = AddPayloadText("Content-Type: multipart/mixed; boundary=" + boundary + "\n"); k++;
		payloadText[k] = AddPayloadText("MIME-Version: 1.0\n"); k++;
		payloadText[k] = AddPayloadText("\n"); k++;
		payloadText[k] = AddPayloadText("This is a multi-part message in MIME format.\n"); k++;
		payloadText[k] = AddPayloadText("\n"); k++;
		payloadText[k] = AddPayloadText("--" + boundary + "\n"); k++;
		payloadText[k] = AddPayloadText("Content-Type: text/plain; charset=ISO-8859-1\n"); k++;
		payloadText[k] = AddPayloadText("Content-Transfer-Encoding: quoted-printable\n"); k++;
	}

	// Normal body
	payloadText[k] = AddPayloadText("\n"); k++;// Empty line to divide headers from body
	for (i = 0; i < messageLines; i++)
	{
		payloadText[k] = AddPayloadText(messageText[i]);
		k++;
	}

	// Special body contents when attaching a file
	if (!imageFileName.empty())
	{
		std::string image = imageFileName.substr(imageFileName.find_last_of('/') + 1);

		payloadText[k] = AddPayloadText("\n"); k++;
		payloadText[k] = AddPayloadText("--" + boundary + "\n"); k++;
		payloadText[k] = AddPayloadText("Content-Type: image/" + GetExtension(imageFileName) + ";\n"); k++;
		payloadText[k] = AddPayloadText("	name=\"" + image + "\"\n"); k++;
		payloadText[k] = AddPayloadText("Content-Transfer-Encoding: base64\n"); k++;
		payloadText[k] = AddPayloadText("Content-Disposition: attachment;\n"); k++;
		payloadText[k] = AddPayloadText("	filename=\"" + image + "\";\n"); k++;

		size_t cr(0), lastCr;
		while (cr != std::string::npos)
		{
			lastCr = cr;
			cr = base64File.find("\n", lastCr + 1);
			payloadText[k] = AddPayloadText(base64File.substr(lastCr,
				std::min(cr - lastCr, (size_t)base64File.size() - lastCr))); k++;
		}
		payloadText[k] = AddPayloadText("--" + boundary + "\n"); k++;
	}

	payloadText[k] = AddPayloadText("\0"); k++;

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
void EmailSender::GenerateMessageText(void)
{
	DeleteMessageText();
	std::stringstream mStream(message);
	std::vector<std::string> mVector;
	std::string line;

	while (!mStream.eof())
	{
		std::getline(mStream, line);
		mVector.push_back(line);
	}

	messageLines = mVector.size();
	messageText = new char*[messageLines];

	unsigned int i;
	for (i = 0; i < messageLines; i++)
		messageText[i] = AddPayloadText(mVector[i] + "\n");
}

//==========================================================================
// Class:			EmailSender
// Function:		DeleteMessageText
//
// Description:		Deletes the message text buffer array.
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
void EmailSender::DeleteMessageText(void)
{
	if (messageText)
	{
		unsigned int i;
		for (i = 0; i < messageLines - 1; i++)
			delete [] messageText[i];
		delete [] messageText;
		messageText = NULL;
	}
}

//==========================================================================
// Class:			EmailSender
// Function:		AddPayloadText
//
// Description:		Adds the specified text to the payload buffer.
//
// Input Arguments:
//		s		= const std::string &s
//
// Output Arguments:
//		None
//
// Return Value:
//		char*
//
//==========================================================================
char* EmailSender::AddPayloadText(const std::string &s) const
{
	char *temp = new char[s.size() + 1];
	memcpy(temp, s.c_str(), s.size());
	temp[s.size()] = '\0';

	return temp;
}

//==========================================================================
// Class:			EmailSender
// Function:		NameToHeaderAddress
//
// Description:		Converts from a name to an address and name (i.e.
//					user@domain (Name)).
//
// Input Arguments:
//		s		= const std::string &s
//
// Output Arguments:
//		None
//
// Return Value:
//		std::string
//
//==========================================================================
std::string EmailSender::NameToHeaderAddress(const std::string &s)
{
	return s + " (" + s + ")";
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
std::string EmailSender::GetDateString(void)
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
std::string EmailSender::GenerateMessageID(void) const
{
	std::string id("<");
	id.append(OAuth2Interface::Base36Encode(GetMillisecondsSinceEpoch()));
	id.append(".");
	id.append(OAuth2Interface::Base36Encode((long long)rand() * (long long)rand()
		* (long long)rand() * (long long)rand()));
	id.append("@");
	id.append(ExtractDomain(loginInfo.localEmail));
	id.append(">");

	return id;
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
std::string EmailSender::GenerateBoundryID(void)
{
	return OAuth2Interface::Base36Encode((long long)rand() * (long long)rand()
		* (long long)rand() * (long long)rand()
		* (long long)rand() * (long long)rand()
		* (long long)rand() * (long long)rand());
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
	struct UploadStatus *uploadCtx = (struct UploadStatus*)userp;
	const char *data;

	if ((size == 0) || (nmemb == 0) || ((size * nmemb) < 1))
		return 0;

	data = uploadCtx->et->payloadText[uploadCtx->linesRead];
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
	std::ifstream inFile(fileName.c_str(), std::ios::in | std::ios::binary);
	if (!inFile.is_open() || !inFile.good())
		return "";

	// FIXME:  Perofrmance could be improved by pre-allocating the return buffer

	inFile >> std::noskipws;
	unsigned char uc;
	std::vector<unsigned char> v;
	while (inFile >> uc, !inFile.eof())
		v.push_back(uc);

	inFile.close();

	const char* charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	unsigned int i;
	lines = 0;
	std::string buf;
	unsigned char oct1, oct2, oct3, hex1, hex2, hex3, hex4;
	for (i = 0; i < v.size(); i += 3)
	{
		oct1 = v[i];
		oct2 = 0;
		oct3 = 0;

		if (i + 1 < v.size())
			oct2 = v[i + 1];
		if (i + 2 < v.size())
			oct3 = v[i + 2];

		hex1 = oct1 >> 2;
		hex2 = ((oct1 & 0x3) << 4) | (oct2 >> 4);
		hex3 = ((oct2 & 0xf) << 2) | (oct3 >> 6);
		hex4 = oct3 & 0x3f;

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
// Function:		GetMillisecondsSinceEpoch
//
// Description:		Returns the current time in "milliseconds since unix epoch."
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		long long
//
//==========================================================================
long long EmailSender::GetMillisecondsSinceEpoch(void)
{
	long long seconds = time(NULL);
	long long msecs;
#ifdef _WIN32
	// msec since system was started - keep only the fractional part
	// Windows doesn't have a similar function, so we just make it up.
	msecs = (long long)GetTickCount64() % 1000LL;
#else
	/*struct timeval tp;
	gettimeofday(&tp);
	long long ms = tp.tv_sec * 1000LL + tp.tv_usec / 1000LL;*/
	// FIXME:  Linux implementation needs work
	// See: http://stackoverflow.com/questions/1952290/how-can-i-get-utctime-in-milisecond-since-january-1-1970-in-c-language
	msecs = 0;
#endif

	return seconds * 1000LL + msecs;
}

//==========================================================================
// Class:			EmailSender
// Function:		DeletePayloadText
//
// Description:		Deletes the payload text buffer.
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
void EmailSender::DeletePayloadText(void)
{
	if (payloadText)
	{
		unsigned int i;
		for (i = 0; i < payloadLines; i++)
			delete [] payloadText[i];
		delete [] payloadText;
		payloadText = NULL;
	}
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
