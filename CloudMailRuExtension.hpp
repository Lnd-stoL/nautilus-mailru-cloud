
#ifndef NAUTILUS_MAILRU_CLOUD_CLOUDMAILRUEXTENSION_H
#define NAUTILUS_MAILRU_CLOUD_CLOUDMAILRUEXTENSION_H

//----------------------------------------------------------------------------------------------------------------------

#include "FileInfo.hpp"
#include "MenuItem.hpp"
#include "GUIProvider.hpp"
#include "CloudMailRuRestAPI.hpp"

#include <unordered_map>
#include <exception>
#include <vector>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <list>

using std::vector;
using std::pair;
using std::function;
using std::list;
using std::unordered_map;

using namespace std::chrono;

#include <boost/property_tree/ptree.hpp>
namespace b_pt = boost::property_tree;

//----------------------------------------------------------------------------------------------------------------------

class CloudMailRuExtension
{
public:
    class Error : public std::exception
    {
    public:
        enum code_t { CLIENT_CONFIG_ERROR, CONFIG_ERROR };

    private:
        code_t _code;
        string _reason;

    public:
        Error(code_t code, string reason = "") : _code(code), _reason(reason)  { }

        inline code_t code() const noexcept {
            return _code;
        }

        virtual const char* what() const noexcept {
            return _reason.c_str();
        }
    };

//----------------------------------------------------------------------------------------------------------------------

private:
    struct _AsyncTask
    {
        string  id;
        function<void()>  task;

        bool operator== (const _AsyncTask& at) {
            return id == at.id;
        }
    };


private:
    GUIProvider  *_gui = nullptr;
    CloudMailRuRestAPI  _cloudAPI;

    string  _localCloudDir;
    string  _mailRuUserName;
    b_pt::ptree  _config;

    unordered_map<string, CloudMailRuRestAPI::CloudFileInfo>  _cachedCloudFiles;

    bool  _running = true;
    list<_AsyncTask>  _asyncTasksQueue;
    std::condition_variable  _tasksUpdateTrigger;
    std::mutex  _tasksAccessLocker;


private:
    bool _readOfficialClientConfig();
    string _configFileName();
    bool _readConfiguration();
    void _saveConfiguration();
    static b_fs::path _userHomeDirPath();
    string _cloudPath(const FileInfo &file);

    void _netUpdateDirectory(const string &dirName);
    bool _updateFileSyncState(FileInfo &file);

    void _enqueueAsyncTask(const string &id, function<void()> task);
    bool _isTaskOnQueue(const string &id);


public:
    CloudMailRuExtension(GUIProvider *guiProvider);
    ~CloudMailRuExtension();

    void getContextMenuItemsForFile(const FileInfo &file, vector<FileContextMenuItem> &items);
    void updateFileInfo(FileInfo *file);
};

//----------------------------------------------------------------------------------------------------------------------

#endif    //NAUTILUS_MAILRU_CLOUD_CLOUDMAILRUEXTENSION_H
