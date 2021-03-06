
#ifndef NAUTILUS_MAILRU_CLOUD_CLOUDMAILRUEXTENSION_H
#define NAUTILUS_MAILRU_CLOUD_CLOUDMAILRUEXTENSION_H

//----------------------------------------------------------------------------------------------------------------------

#include "FileInfo.hpp"
#include "MenuItem.hpp"
#include "GUIProvider.hpp"
#include "CloudMailRuRestAPI.hpp"
#include "ExtensionError.hpp"

#include <unordered_map>
#include <exception>
#include <vector>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <set>
#include <unordered_set>

using std::vector;
using std::pair;
using std::function;
using std::list;

using namespace std::chrono;

#include <boost/property_tree/ptree.hpp>
namespace b_pt = boost::property_tree;

//----------------------------------------------------------------------------------------------------------------------

class CloudMailRuExtension
{
private:
    struct _AsyncTask
    {
        unsigned long  taskNumber;
        time_point<system_clock>  startTime;
        string  id;
        function<void()>  task;


        bool operator< (const _AsyncTask& at) const {
            if (startTime == at.startTime)
                return taskNumber < at.taskNumber;
            return startTime < at.startTime;
        }
    };


    struct _CachedFolderInfo
    {
        time_point<system_clock>  lastUpdateTime;
        time_point<system_clock>  lastAskedToUpdateTime;
    };


private:
    GUIProvider  *_gui = nullptr;
    CloudMailRuRestAPI  _cloudAPI;

    string  _localCloudDir;
    string  _mailRuUserName;
    b_pt::ptree  _config;

    std::unordered_map<string, CloudMailRuRestAPI::CloudFileInfo>  _cachedCloudFiles;
    std::unordered_map<string, _CachedFolderInfo>                  _cachedCloudFolders;

    std::set<string>  _updateActiveFolders;

    bool  _running = true;
    unsigned long  _taskCounter = 1;
    std::set<_AsyncTask>  _asyncTasksQueue;
    std::unordered_set<string>  _asyncTasksSet;
    std::condition_variable  _tasksUpdateTrigger;
    std::mutex  _tasksAccessLocker;


private:
    bool _readOfficialClientConfig();
    string _configFileName();
    bool _readConfiguration();
    void _writeDefaultConfig();
    void _ensureCloudAPIIsReady();
    void _saveConfiguration();
    bool _isOneOfCloudHiddenSystemFiles(const string& cloudFilePath);
    static b_fs::path _userHomeDirPath();
    string _cloudPath(const FileInfo &file);

    void _folderUpdateTask(const string &dirName);
    bool _planDelayedFolderUpdate(const string &dirName, int updateTimeoutMs);
    bool _updateFileSyncState(FileInfo &file);

    void _enqueueAsyncTask(const string &id, time_point<system_clock> startTime, function<void()> task);
    bool _isTaskOnQueue(const string &id);

    void _fileUpdateTask(FileInfo *file, string fileCloudDir, int sequenceNum);


public:
    CloudMailRuExtension(GUIProvider *guiProvider);
    ~CloudMailRuExtension();

    // This methods are designed to take ownership of FileInfo and the responsibility to release it.
    // That was done in order to enable asynchrounus operations with FileInfo.
    void getContextMenuItemsForFile(FileInfo *file, vector<FileContextMenuItem> &items);
    void updateFileInfo(FileInfo *file);


public:
    inline const b_pt::ptree& config() const {
        return _config;
    }
};

//----------------------------------------------------------------------------------------------------------------------

#endif    //NAUTILUS_MAILRU_CLOUD_CLOUDMAILRUEXTENSION_H
