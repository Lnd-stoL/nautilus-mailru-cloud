
#include "CloudMailRuRestAPI.hpp"
#include "URLTools.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/network/uri.hpp>

namespace b_network = boost::network;
namespace b_uri     = b_network::uri;
namespace b_pt      = boost::property_tree;

using b_network::header;
using b_network::body;

//----------------------------------------------------------------------------------------------------------------------

void CloudMailRuRestAPI::_testLoggedIn()
{
    if (!_loggedIn) {
        throw 5;
    }
}


string CloudMailRuRestAPI::getPublicLinkTo(const string &cloudItemPath)
{
    _testLoggedIn();

    b_http::client::request apiRequest = _createRequestWithDefaultHdrs("https://cloud.mail.ru/api/v2/file/publish");
    apiRequest << header("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");

    std::ostringstream formPostFields;
    formPostFields << "home="   << URLTools::encode(cloudItemPath)
                   << "&token=" << URLTools::encode(_apiToken);

    std::cout << formPostFields.str() << std::endl;

    auto apiResponse = _httpClient.post(apiRequest, formPostFields.str());
    std::stringstream responseText;
    responseText << apiResponse.body();

    b_pt::ptree responseJSON;
    b_pt::read_json(responseText, responseJSON);

    return string("https://cloud.mail.ru/public/") + responseJSON.get<string>("body");
}


bool CloudMailRuRestAPI::login(const string &login, const string &password)
{
    b_http::client::request authRequest = _createRequestWithDefaultHdrs("http://auth.mail.ru/cgi-bin/auth");
    authRequest << header("Content-Type","application/x-www-form-urlencoded");

    std::ostringstream formPostFields;
    formPostFields << "page="      << URLTools::encode("http://cloud.mail.ru/")
                   << "&Login="    << URLTools::encode(login)
                   << "&Password=" << URLTools::encode(password);

    auto authResponse = _httpClient.post(authRequest, formPostFields.str());
    if (authResponse.status() != 302 /*Redirection*/) {
        // TODO: error handling
    }
    _storeCookies(authResponse);

    _loggedIn = _requestAPIToken();
    return _loggedIn;
}


b_http::client::request CloudMailRuRestAPI::_createRequestWithDefaultHdrs(const string &uri)
{
    b_http::client::request req(uri);

    if (!_cookies.empty()) {
        std::ostringstream cookieHeader;

        for (const auto& cookie: _cookies) {
            cookieHeader << cookie.first << '=' << cookie.second << "; ";
        }

        req << header("Cookie", cookieHeader.str());
        std::cout << "ccc: " << cookieHeader.str() << std::endl;
    }

    req << header("User-Agent", _userAgent);
    return req;
}


void CloudMailRuRestAPI::_storeCookies(const b_http::client::response &response)
{
    auto setCookieHeaders = response.headers().equal_range("Set-Cookie");
    for (auto cookieIt = setCookieHeaders.first; cookieIt != setCookieHeaders.second; ++cookieIt) {

        int nameEnd = cookieIt->second.find('=');
        int valueBegin = nameEnd + 1;
        int valueEnd = cookieIt->second.find(';', valueBegin);

        _cookies[cookieIt->second.substr(0, nameEnd)] = cookieIt->second.substr(valueBegin, valueEnd - valueBegin);
    }
}


void CloudMailRuRestAPI::getFolderContents(const string &cloudFolderPath, vector<CloudFileInfo> &items)
{
    _testLoggedIn();

    b_http::client::request apiRequest = _createRequestWithDefaultHdrs("https://cloud.mail.ru/api/v2/folder");
    apiRequest << header("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");

    std::ostringstream formPostFields;
    formPostFields << "home="   << URLTools::encode(cloudFolderPath)
                   << "&token=" << URLTools::encode(_apiToken);

    auto apiResponse = _httpClient.post(apiRequest, formPostFields.str());
    std::stringstream responseText;
    responseText << apiResponse.body();

    b_pt::ptree responseJSON;
    b_pt::read_json(responseText, responseJSON);

    int itemCount = responseJSON.get<int>("body.count.files") + responseJSON.get<int>("body.count.folders");
    items.reserve(itemCount);

    auto &listJSON = responseJSON.get_child("body.list");
    for (auto &fileJSON : listJSON) {

        CloudFileInfo fileInfo;
        fileInfo.path  = fileJSON.second.get<string>("home");
        fileInfo.size  = fileJSON.second.get<unsigned>("size");
        fileInfo.directory = fileJSON.second.get<string>("kind") != "file";

        if (!fileInfo.directory) {
            fileInfo.mtime = fileJSON.second.get<unsigned>("mtime");
        }

        if (fileJSON.second.find("weblink") != fileJSON.second.not_found()) {
            fileInfo.weblink = fileJSON.second.get<string>("weblink");
        }

        items.push_back(std::move(fileInfo));
    }
}


void CloudMailRuRestAPI::removePublicLinkTo(const string &itemWeblink)
{
    _testLoggedIn();

    b_http::client::request apiRequest = _createRequestWithDefaultHdrs("https://cloud.mail.ru/api/v2/file/unpublish");
    apiRequest << header("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");

    std::ostringstream formPostFields;
    formPostFields << "weblink=" << itemWeblink
                   << "&token="  << URLTools::encode(_apiToken);

    auto apiResponse = _httpClient.post(apiRequest, formPostFields.str());
    assert( apiResponse.status() == 200 );
}


void CloudMailRuRestAPI::storeCookies(b_pt::ptree &config)
{
    _testLoggedIn();

    for (auto cookie : _cookies) {
        config.put(cookie.first, cookie.second);
    }
}


bool CloudMailRuRestAPI::loginWithCookies(const b_pt::ptree &config)
{
    for (auto &cookieConfig : config) {
        _cookies[cookieConfig.first] = cookieConfig.second.data();
    }

    if (_cookies.empty() || _cookies.find("Mpop") == _cookies.end()) {
        return false;
    }

    for (auto c : _cookies)
        std::cout << c.first << " = " << c.second << std::endl;

    _loggedIn = _requestAPIToken();
    return _loggedIn;
}


bool CloudMailRuRestAPI::_requestAPIToken()
{
    b_http::client::request cloudHomeRequest = _createRequestWithDefaultHdrs("https://cloud.mail.ru");
    cloudHomeRequest << header("Host", "cloud.mail.ru");
    cloudHomeRequest << header("Accept", "*/*");
    auto cloudHomeResponse = _httpClient.get(cloudHomeRequest);

    if (cloudHomeResponse.status() != 200) {
        std::cout << extension_info::logPrefix << "warning: API token request failed (request status is not 200 OK but is "
            << cloudHomeResponse.status() << " instead)" << std::endl;
        return false;
    }

    const char *tokenSearchPattern = "\"token\": \"";
    auto tokenPos = cloudHomeResponse.body().find(tokenSearchPattern);
    if (tokenPos == string::npos) {
        std::cout << extension_info::logPrefix << "warning: API token request failed (no token found in response)" << std::endl;
        //std::cout << extension_info::logPrefix << "response body was: " <<  cloudHomeResponse.body() << std::endl;
        return false;
    }

    auto tokenValBegin = tokenPos + (int) strlen(tokenSearchPattern);
    auto tokenValEnd = cloudHomeResponse.body().find('"', tokenValBegin);
    _apiToken = cloudHomeResponse.body().substr(tokenValBegin, tokenValEnd - tokenValBegin);

    return true;
}
