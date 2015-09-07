
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
    b_http::basic_client<b_http::tags::http_async_8bit_udp_resolve, 1, 1> _httpClient;

    std::unordered_map<string, string> _cookies;
    string _apiToken;
    bool _loggedIn = false;


private:
    b_http::client::request _createRequestWithDefaultHdrs(const string &uri);
    void _storeCookies(const b_http::client::response &response);
    bool _requestAPIToken();

    bool _testLoggedIn();
    void _reportHTTPStatusError(int responseStatus, const string &requestInfo);
    void _reportException(boost::system::system_error &error, const string &requestInfo);


public:
    bool loginWithCookies(const b_pt::ptree &config);
    void storeCookies(b_pt::ptree &config);
    bool login(const string &login, const string &password);

    // returns empty string if something goes wrong
    string getPublicLinkTo(const string &cloudItemPath);
    bool removePublicLinkTo(const string &itemWeblink);
    bool getFolderContents(const string &cloudFolderPath, vector<CloudFileInfo> &items);


public:
    inline bool loggedIn() const {
        return _loggedIn;
    }
};

//----------------------------------------------------------------------------------------------------------------------

#endif    //NAUTILUS_MAILRU_CLOUD_CLOUDMAILRUHTTPAPI_H
