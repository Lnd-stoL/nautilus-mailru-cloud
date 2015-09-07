
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

bool CloudMailRuRestAPI::_testLoggedIn()
{
    if (!_loggedIn) {
        std::cout << extension_info::logPrefix << "warning: cloud rest API method call without logged in user - skipping"
            << std::endl;
    }

    return _loggedIn;
}


string CloudMailRuRestAPI::getPublicLinkTo(const string &cloudItemPath)
{
    if (!_testLoggedIn())  return "";

    b_http::client::request apiRequest = _createRequestWithDefaultHdrs("https://cloud.mail.ru/api/v2/file/publish");
    apiRequest << header("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");

    std::ostringstream formPostFields;
    formPostFields << "home="   << URLTools::encode(cloudItemPath)
                   << "&token=" << URLTools::encode(_apiToken);

    std::stringstream responseText;
    try {
        auto apiResponse = _httpClient.post(apiRequest, formPostFields.str());
        if (apiResponse.status() != 200) {
            _reportHTTPStatusError(apiResponse.status(), "get public link");
            return "";
        }
        responseText << apiResponse.body();
    } catch (boost::exception &error) {
        _reportBoostException(error, "getting public link");
        return "";
    }

    b_pt::ptree responseJSON;
    b_pt::read_json(responseText, responseJSON);

    if (responseJSON.find("body") == responseJSON.not_found()) {
        std::cout << extension_info::logPrefix << "error: cloud rest API (get public link) - unsusual JSON answer: "
            << responseText.str() << std::endl;
        return "";
    }

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

    try {
        auto authResponse = _httpClient.post(authRequest, formPostFields.str());
        if (authResponse.status() != 302 /*Redirection*/) {
            std::cout << extension_info::logPrefix << "warning: cloud rest API - login - auth page status code is not redirection" << std::endl;
            return false;
        }
        _storeCookies(authResponse);
    } catch (boost::exception &error) {
        _reportBoostException(error, "logging in");
        return false;
    }

    if (_cookies.find("Mpop") == _cookies.end())
        return false;    // seems invalid password or username

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


bool CloudMailRuRestAPI::getFolderContents(const string &cloudFolderPath, vector<CloudFileInfo> &items)
{
    if (!_testLoggedIn())  return false;

    b_http::client::request apiRequest = _createRequestWithDefaultHdrs("https://cloud.mail.ru/api/v2/folder");
    apiRequest << header("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");

    std::ostringstream formPostFields;
    formPostFields << "home="   << URLTools::encode(cloudFolderPath)
                   << "&token=" << URLTools::encode(_apiToken);

    std::stringstream responseText;
    try {
        auto apiResponse = _httpClient.post(apiRequest, formPostFields.str());
        if (apiResponse.status() != 200) {
            _reportHTTPStatusError(apiResponse.status(), "get folder contents");
            return false;
        }
        responseText << apiResponse.body();
    } catch (boost::exception &error) {
        _reportBoostException(error, "getting folder contents");
        return false;
    }

    b_pt::ptree responseJSON;
    b_pt::read_json(responseText, responseJSON);

    try {
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
    } catch (boost::exception &error) {
        _reportBoostException(error, "getting folder contents");
        return false;
    }

    return true;
}


bool CloudMailRuRestAPI::removePublicLinkTo(const string &itemWeblink)
{
    if (!_testLoggedIn())  return false;

    b_http::client::request apiRequest = _createRequestWithDefaultHdrs("https://cloud.mail.ru/api/v2/file/unpublish");
    apiRequest << header("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");

    std::ostringstream formPostFields;
    formPostFields << "weblink=" << itemWeblink
                   << "&token="  << URLTools::encode(_apiToken);

    try {
        auto apiResponse = _httpClient.post(apiRequest, formPostFields.str());
        if (apiResponse.status() != 200) {
            _reportHTTPStatusError(apiResponse.status(), "remove public link");
            return false;
        }
    } catch (boost::exception &error) {
        _reportBoostException(error, "removing public link");
        return false;
    }

    return true;
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

    _loggedIn = _requestAPIToken();
    return _loggedIn;
}


bool CloudMailRuRestAPI::_requestAPIToken()
{
    b_http::client::request cloudHomeRequest = _createRequestWithDefaultHdrs("https://cloud.mail.ru");
    cloudHomeRequest << header("Host", "cloud.mail.ru");
    cloudHomeRequest << header("Accept", "*/*");

    string responseBody = "";
    try {
        auto cloudHomeResponse = _httpClient.get(cloudHomeRequest);
        if (cloudHomeResponse.status() != 200) {
            std::cout << extension_info::logPrefix << "warning: API token request failed (request status is not 200 OK but is "
                << cloudHomeResponse.status() << " instead)" << std::endl;
            return false;
        }
        responseBody = cloudHomeResponse.body();
    } catch (boost::exception &error) {
        _reportBoostException(error, "requesting API token");
        return false;
    }

    const char *tokenSearchPattern = "\"token\": \"";
    auto tokenPos = responseBody.find(tokenSearchPattern);
    if (tokenPos == string::npos) {
        std::cout << extension_info::logPrefix << "warning: API token request failed (no token found in response)" << std::endl;
        //std::cout << extension_info::logPrefix << "response body was: " <<  cloudHomeResponse.body() << std::endl;
        return false;
    }

    auto tokenValBegin = tokenPos + (int) strlen(tokenSearchPattern);
    auto tokenValEnd = responseBody.find('"', tokenValBegin);
    _apiToken = responseBody.substr(tokenValBegin, tokenValEnd - tokenValBegin);

    return true;
}


void CloudMailRuRestAPI::_reportHTTPStatusError(int responseStatus, const string &requestInfo)
{
    if (responseStatus == 403) {
        std::cout << extension_info::logPrefix << "error: cloud rest API failed to " << requestInfo << "- 403 forbidden" << std::endl;
    } else if (responseStatus == 404) {
        std::cout << extension_info::logPrefix << "error: cloud rest API failed to " << requestInfo << "- 404 not found" << std::endl;
    } else if (responseStatus == 507) {
        std::cout << extension_info::logPrefix << "error: cloud rest API failed to " << requestInfo << "- 507 storage exceeded" << std::endl;
    } else {
        std::cout << extension_info::logPrefix << "error: cloud rest API failed to " << requestInfo << " - status " << responseStatus << std::endl;
    }
}


void CloudMailRuRestAPI::_reportBoostException(boost::exception& error, const string &requestInfo)
{
    std::cout << extension_info::logPrefix << "error: exception thrown while "
        << requestInfo << ": " << boost::diagnostic_information(error) << std::endl;
}
