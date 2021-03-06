# Commonly Used Email Classes

Date:       5/14/2015  
Author:     K. Loux  
Copyright:  Kerry Loux 2015  
Licence:    MIT (see LICENSE file)  

This collection of classes are used in several of my projects, so I decided to break them out as a submodule for ease of maintenance.  This file contains some notes about using the classes contained in this repository.

There are several guides online for using git submodules.  Here is my summary of the most useful git commands.
- To add email to your project as a submodule:
````
$ cd <root directory of your superproject>
$ git submodule add https://github.com/KerryL/email.git
````

NOTE:  To add a submodule within another directory, the destination must be specified following the repository url, so instead of the last step above, it would be:
````
$ git submodule add https://github.com/KerryL/email.git <desired path>/email
````

- Cloning a repository using a submodule now requires an extra step:
````
$ git clone ...
$ cd <project directory created by above clone command>
$ git submodule update --init --recursive
````

## Notes on sending email with these classes
These classes are designed to be used with Gmail via Google's OAuth2 implementation.  In order to use these classes with Gmail, some setup is required with your Google account.  You will need to do the following:
1.  Visit https://console.developers.google.com/
2.  Create a new project and go through the setup process.
	*  Add the "https://mail.google.com/" scope (see below; in the future this repo may be updated to make use of the "../auth/gmail.send" scope instead).
	*  Add a Test user.
3.  From the Dashboard, click on "Enable APIs and Services" and enable the Gmail API.
4.  From the Credentials page, click on "Create Credentials" and select "OAuth Client ID".  Select "Desktop App" for the Application Type.
5.  Note the Client ID and Client secret; these fields need to be passed to the OAuth2Interface singleton as shown below.  The sender's email address must match the test user configured in step 2.

Also, note that the OAuth2Interface class is implemented in a generic way.  It is up to the develper to specify the correct URLs, grant types, scopes, etc.  One possible way to make calls to set up the interface is

```C++
    OAuth2Interface::Get().SetClientID(email.oAuth2ClientID);
    OAuth2Interface::Get().SetClientSecret(email.oAuth2ClientSecret);
    OAuth2Interface::Get().SetTokenURL(_T("https://accounts.google.com/o/oauth2/token"));
    OAuth2Interface::Get().SetAuthenticationURL(_T("https://accounts.google.com/o/oauth2/auth"));
    OAuth2Interface::Get().SetResponseType(_T("code"));
    OAuth2Interface::Get().SetRedirectURI(_T("urn:ietf:wg:oauth:2.0:oob"));
    OAuth2Interface::Get().SetLoginHint(email.sender);// This is the sender's email address
    OAuth2Interface::Get().SetGrantType(_T("authorization_code"));
    OAuth2Interface::Get().SetScope(_T("https://mail.google.com/"));
```

Note that the scope used is "https://mail.google.com/" and not one of the other more granular scopes available from Google.  Those other scopes are only valid when using Gmail's REST interface; the current EmailSender implementation relies on SMTP and must be granted access to the full scope of the user's email.  This may be changed in the future.

It is also possible to set up the interface for limited-input devices like this:

```C++
    OAuth2Interface::Get().SetClientID(email.oAuth2ClientID);
    OAuth2Interface::Get().SetClientSecret(email.oAuth2ClientSecret);
    OAuth2Interface::Get().SetTokenURL(_T("https://www.googleapis.com/oauth2/v3/token"));
    OAuth2Interface::Get().SetAuthenticationURL(_T("https://accounts.google.com/o/oauth2/device/code"));
    OAuth2Interface::Get().SetAuthenticationPollURL(_T("https://oauth2.googleapis.com/token"));
    OAuth2Interface::Get().SetGrantType(_T("http://oauth.net/grant_type/device/1.0"));
    OAuth2Interface::Get().SetPollGrantType(_T("urn:ietf:params:oauth:grant-type:device_code"));
    OAuth2Interface::Get().SetScope(_T("email"));
```

Note, however, when using limited-input devices, the "email" scope does not support sending email, so the first method must be used if the goal is to send email.  For other purposes, the OAuth2Interface class can be used with any scope to successfully pull refresh and access tokens.
