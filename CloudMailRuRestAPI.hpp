
#ifndef NAUTILUS_MAILRU_CLOUD_CLOUDMAILRUHTTPAPI_H
#define NAUTILUS_MAILRU_CLOUD_CLOUDMAILRUHTTPAPI_H

//----------------------------------------------------------------------------------------------------------------------

#define BOOST_NETWORK_ENABLE_HTTPS
#include <boost/network/protocol/http.hpp>
namespace b_http = boost::network::http;

#include <boost/property_tree/ptree.hpp>
namespace b_pt = boost::property_tree;

#include <string>
#include <unordered_map>
#include <vector>

using std::string;
using std::vector;

#include "extension_info.hpp"

//----------------------------------------------------------------------------------------------------------------------

class CloudMailRuRestAPI
{
public:
    struct CloudFileInfo
    {
        unsigned  mtime;
        unsigned  size;
        bool      directory;
        string    path;
        string    weblink;
    };


private:
    string _userAgent = string("nautilus-cloud-mailru-extension/") + extension_info::versionString + " ( Linux x86_64 )";
    b_http::client _httpClient;

    std::unordered_map<string, string> _cookies;
    string _apiToken;
    bool _loggedIn = false;


private:
    b_http::client::request _requestWithDefaultHdrs(const string &uri, bool includeCookies = false);
    void _storeCookies(const b_http::client::response &response);
    void _testLoggedIn();
    bool _requestAPIToken();

public:
    bool loginWithCookies(const b_pt::ptree &config);
    void storeCookies(b_pt::ptree &config);
    bool login(const string &login, const string &password);

    string getPublicLinkTo(const string &cloudItemPath);
    void removePublicLinkTo(const string &itemWeblink);
    void getFolderContents(const string &cloudFolderPath, vector<CloudFileInfo> &items);


public:
    inline bool loggedIn() const {
        return _loggedIn;
    }
};

//----------------------------------------------------------------------------------------------------------------------

#endif    //NAUTILUS_MAILRU_CLOUD_CLOUDMAILRUHTTPAPI_H
