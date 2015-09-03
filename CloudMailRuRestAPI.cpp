
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

string CloudMailRuRestAPI::getPublicLinkTo(const string &cloudItemPath)
{
    b_http::client::request apiRequest = _requestWithDefaultHdrs("https://cloud.mail.ru/api/v2/file/publish", true);
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


void CloudMailRuRestAPI::login(const string &login, const string &password)
{
    b_http::client::request authRequest = _requestWithDefaultHdrs("http://auth.mail.ru/cgi-bin/auth");
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

    b_http::client::request cloudHomeRequest = _requestWithDefaultHdrs("https://cloud.mail.ru", true);
    auto cloudHomeResponse = _httpClient.get(cloudHomeRequest);

    const char *tokenSearchPattern = "\"token\": \"";
    auto tokenPos = cloudHomeResponse.body().find(tokenSearchPattern);
    if (tokenPos == string::npos) {
        // TODO: error handling
        std::cout << "fuck!!!";
    }

    auto tokenValBegin = tokenPos + (int) strlen(tokenSearchPattern);
    auto tokenValEnd = cloudHomeResponse.body().find('"', tokenValBegin);
    _apiToken = cloudHomeResponse.body().substr(tokenValBegin, tokenValEnd - tokenValBegin);
}


b_http::client::request CloudMailRuRestAPI::_requestWithDefaultHdrs(const string &uri, bool includeCookies)
{
    b_http::client::request req(uri);
    req /* << header("Connection", "close") */
        << header("User-Agent", _userAgent);

    if (includeCookies) {
        std::ostringstream cookieHeader;

        for (const auto& cookie: _cookies) {
            cookieHeader << cookie.first << '=' << cookie.second << "; ";
        }

        req << header("Cookie:", cookieHeader.str());
    }

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
    b_http::client::request apiRequest = _requestWithDefaultHdrs("https://cloud.mail.ru/api/v2/folder", true);
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
    b_http::client::request apiRequest = _requestWithDefaultHdrs("https://cloud.mail.ru/api/v2/file/unpublish", true);
    apiRequest << header("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");

    std::ostringstream formPostFields;
    formPostFields << "weblink=" << itemWeblink
                   << "&token="  << URLTools::encode(_apiToken);

    auto apiResponse = _httpClient.post(apiRequest, formPostFields.str());
    assert( apiResponse.status() == 200 );
}
