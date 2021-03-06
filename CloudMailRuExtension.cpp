
#include "CloudMailRuExtension.hpp"
#include "extension_info.hpp"

#include <unistd.h>
#include <pwd.h>
#include <cassert>
#include <thread>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ini_parser.hpp>

namespace b_fs = boost::filesystem;

//----------------------------------------------------------------------------------------------------------------------

CloudMailRuExtension::CloudMailRuExtension(GUIProvider *guiProvider) :
    _gui(guiProvider)
{
    _readOfficialClientConfig();
    _readConfiguration();

    std::thread([this]() {
        while (_running) {
            std::unique_lock<std::mutex> lock(_tasksAccessLocker);

            if (_asyncTasksQueue.empty()) {
                _tasksUpdateTrigger.wait(lock);
            } else {
                _tasksUpdateTrigger.wait_until(lock, _asyncTasksQueue.begin()->startTime);
            }

            while (!_asyncTasksQueue.empty() &&
                    _asyncTasksQueue.begin()->startTime <= system_clock::now()) {

                auto nextTaskIt = _asyncTasksQueue.begin();
                lock.unlock();
                _ensureCloudAPIIsReady();    // TODO: really not every net task require mailru API (just a quick fix for login dialog to be in focus)
                nextTaskIt->task();
                lock.lock();

                _asyncTasksSet.erase(nextTaskIt->id);
                _asyncTasksQueue.erase(nextTaskIt);
            }
        }
    }).detach();
}


void CloudMailRuExtension::getContextMenuItemsForFile(FileInfo *file, vector<FileContextMenuItem> &items)
{
    if (file->path().find(_localCloudDir) != 0)
        return;    // no items outside cloud dir

    string fileName = file->pathInfo().filename().string();
    string fileCloudPath = _cloudPath(*file);

    if (_isOneOfCloudHiddenSystemFiles(fileCloudPath))
        return;    // system hidden files are never synced

    // get public link menu item
    static const string publicLinkText = "Копировать ссылку на '";
    items.emplace_back(publicLinkText + fileName + '\'', "GetPublicLink");

    auto getLinkTask =  [this, file, fileCloudPath, fileName]() {
        string link = _cloudAPI.getPublicLinkTo(fileCloudPath);
        std::cout << extension_info::logPrefix << "got publink link to " << fileCloudPath << " : " << link << std::endl;

        _gui->invokeInGUIThread([this, file, fileName, link, fileCloudPath]() {
            if (link != "") {
                _gui->copyToClipboard(link);
                _gui->showCopyPublicLinkNotification(string("Общедоступная ссылка на '") + fileName + "' скопированна в буфер обмена.");
                _cachedCloudFiles[fileCloudPath].weblink = link;    // manually emulate thuis change to avoid net request
                file->invalidateExtensionInfo();
            } else {
                _gui->showCopyPublicLinkNotification(string("Не удалось скопировать ссылку на '") + fileName + "'.");
            }
            delete file;
        });
    };

    items.back().onClick(
        [this, fileCloudPath, getLinkTask]() {
            _enqueueAsyncTask(fileCloudPath + "#weblink-get", system_clock::now(), getLinkTask);
        });


    // remove public link menu item
    if (_cachedCloudFiles[fileCloudPath].weblink != "") {
        items.emplace_back("Прекратить общий доступ", "RemovePublicLink");

        auto removeLinkTask =  [this, file, fileCloudPath, fileName]() {
            bool success = _cloudAPI.removePublicLinkTo(_cachedCloudFiles[fileCloudPath].weblink);

            _gui->invokeInGUIThread([this, file, fileName, fileCloudPath, success]() {
                if (success) {
                    _gui->showCopyPublicLinkNotification(string("Ссылка общего доступа на '") + fileName + "' удалена.");
                    _cachedCloudFiles[fileCloudPath].weblink = "";    // manually emulate thuis change to avoid net request
                    file->invalidateExtensionInfo();
                } else {
                    _gui->showCopyPublicLinkNotification(string("Не удалось удалить ссылку общего доступа на '") + fileName + "'.");
                }
                delete file;
            });
        };

        items.back().onClick(
            [this, fileCloudPath, removeLinkTask]() {
                _enqueueAsyncTask(fileCloudPath + "#weblink-remove", system_clock::now(), removeLinkTask);
            });
    }
}


bool CloudMailRuExtension::_readOfficialClientConfig()
{
    b_pt::ptree cloudClientConfig;
    try {
        b_pt::read_ini((_userHomeDirPath() / ".config/Mail.Ru/Mail.Ru_Cloud.conf").string(), cloudClientConfig);
        _localCloudDir = cloudClientConfig.get<string>("General.folder");
        _mailRuUserName = cloudClientConfig.get<string>("General.email");
        std::cout << extension_info::logPrefix << "found local cloud dir at " << _localCloudDir << std::endl;
    }
    catch (const std::exception &parserErr) {
        std::cout << extension_info::logPrefix << "error while reading official cloud client config: " << parserErr.what() << std::endl;
        throw ExtensionError(ExtensionError::CLIENT_CONFIG_ERROR, string("exception thrown: ") + parserErr.what());
    }

    return true;
}


void CloudMailRuExtension::updateFileInfo(FileInfo *file)
{
    if (file->path().find(_localCloudDir) != 0)
        return;    // nothing to do outside cloud dir

    if (b_fs::is_directory(file->path())) {
        return;    // nothing to do with directories (yet)
    }

    string fileCloudPath = _cloudPath(*file);
    string fileCloudDir = b_fs::path(fileCloudPath).parent_path().string();
    _cachedCloudFolders[fileCloudDir].lastAskedToUpdateTime = system_clock::now();

    if (_isOneOfCloudHiddenSystemFiles(fileCloudPath))
        return;    // system hidden files are never synced

    // now update active folders set
    if (_updateActiveFolders.size() >= _config.get<int>("SyncTweaks.max_active_folders")) {
        // we need to push out some dir
        auto outCandidate = _updateActiveFolders.begin();
        for (auto i = _updateActiveFolders.begin(); i != _updateActiveFolders.end(); ++i) {
            if (_cachedCloudFolders[*outCandidate].lastAskedToUpdateTime >
                _cachedCloudFolders[*i].lastAskedToUpdateTime) {
                outCandidate = i;
            }
        }
        _updateActiveFolders.erase(outCandidate);
    }
    _updateActiveFolders.insert(fileCloudDir);

    if (!_isTaskOnQueue(fileCloudDir)) {    // this is to prevent directory net update for each file in a folder
        _enqueueAsyncTask(fileCloudDir, system_clock::now(),
                          std::bind(&CloudMailRuExtension::_folderUpdateTask, this, fileCloudDir));
    }

    _enqueueAsyncTask(fileCloudDir + "#" + fileCloudPath, system_clock::now(),
                      std::bind(&CloudMailRuExtension::_fileUpdateTask, this, file, fileCloudDir, 0));
}


/*static*/ b_fs::path CloudMailRuExtension::_userHomeDirPath()
{
    const char *homedir = nullptr;

    if ((homedir = ::getenv("HOME")) == NULL) {
        homedir = ::getpwuid(getuid())->pw_dir;
    }

    return b_fs::path(homedir);
}


string CloudMailRuExtension::_cloudPath(const FileInfo& file)
{
    assert( boost::starts_with(file.path(), _localCloudDir) );
    return file.path().substr(_localCloudDir.length());
}


void CloudMailRuExtension::_folderUpdateTask(const string &dirName)
{
    if (duration_cast<milliseconds>(system_clock::now() - _cachedCloudFolders[dirName].lastUpdateTime).count() <
            _config.get<int>("SyncTweaks.min_update_interval")) {
        std::cout << extension_info::logPrefix << "directory update skipped for " << dirName << std::endl;
        return;
    }

    std::cout << extension_info::logPrefix << "started getting direcory contents for " << dirName << std::endl;

    vector<CloudMailRuRestAPI::CloudFileInfo> dirItems;
    if (!_cloudAPI.getFolderContents(dirName, dirItems))
        return;

    for (auto &nextItem : dirItems) {
        _cachedCloudFiles[nextItem.path] = nextItem;
    }

    _cachedCloudFolders[dirName].lastUpdateTime = system_clock::now();

    std::cout << extension_info::logPrefix << "updated directory " << dirName
                << " at " << system_clock::now().time_since_epoch().count() << std::endl;
}


// returns true if subsequent updates are required
bool CloudMailRuExtension::_updateFileSyncState(FileInfo &file)
{
    string fileCloudPath = _cloudPath(file);

    auto cachedFileInfo = _cachedCloudFiles.find(fileCloudPath);
    if (cachedFileInfo == _cachedCloudFiles.end()) {
        file.setSyncState(FileInfo::IN_PROGRESS);
    } else {
        if (std::abs(b_fs::last_write_time(file.pathInfo()) - cachedFileInfo->second.mtime) < 1) {
            if (cachedFileInfo->second.weblink != "") {
                file.setSyncState(FileInfo::ACTUAL_SHARED);
            } else {
                file.setSyncState(FileInfo::ACTUAL);
            }
        } else {
            file.setSyncState(FileInfo::IN_PROGRESS);
        }
    }

    return file.syncState() == FileInfo::IN_PROGRESS;
}


void CloudMailRuExtension::_enqueueAsyncTask(const string &id, time_point<system_clock> startTime, function<void()> task)
{
    _tasksAccessLocker.lock();

    _AsyncTask newTask = {};
    newTask.id = id;
    newTask.task = task;
    newTask.startTime = startTime;
    newTask.taskNumber = _taskCounter++;

    _asyncTasksQueue.insert(std::move(newTask));
    _asyncTasksSet.insert(id);

    _tasksAccessLocker.unlock();
    _tasksUpdateTrigger.notify_all();
}


bool CloudMailRuExtension::_readConfiguration()
{
    if (!b_fs::exists(_configFileName())) {
        _writeDefaultConfig();
        _saveConfiguration();
        return true;
    }

    try {
        b_pt::read_ini(_configFileName(), _config);
    }
    catch (const std::exception &parserErr) {
        std::cout << extension_info::logPrefix << "error while parsing config file: " << parserErr.what() << std::endl;
        throw ExtensionError(ExtensionError::CONFIG_ERROR, string("exception thrown while parsing config: ") + parserErr.what());
    }

    return true;
}


void CloudMailRuExtension::_saveConfiguration()
{
    try {
        b_pt::write_ini(_configFileName(), _config);
    }
    catch (const std::exception &parserErr) {
        std::cout << extension_info::logPrefix << "error while saving config file: " << parserErr.what() << std::endl;
        throw ExtensionError(ExtensionError::CONFIG_ERROR, string("exception thrown while saving config: ") + parserErr.what());
    }
}


string CloudMailRuExtension::_configFileName()
{
    return (_userHomeDirPath() / ".config/Mail.Ru/Mail.Ru_Cloud-NautilusExtension.conf").string();
}


bool CloudMailRuExtension::_isTaskOnQueue(const string &id)
{
    std::unique_lock<std::mutex> lock(_tasksAccessLocker);
    return _asyncTasksSet.find(id) != _asyncTasksSet.end();
}


CloudMailRuExtension::~CloudMailRuExtension()
{
    _running = false;
    _tasksUpdateTrigger.notify_all();
}


void CloudMailRuExtension::_fileUpdateTask(FileInfo *file, string fileCloudDir, int sequenceNum)
{
    int updateTimeoutMs = _config.get<int>("SyncTweaks.in_progress_update_interval");
    updateTimeoutMs += rand() % 1000;
    updateTimeoutMs += sequenceNum * 1000;

    if (!file->isStillActual() || sequenceNum > _config.get<int>("SyncTweaks.max_file_updates")) {
        delete file;
        return;
    }

    if (_updateFileSyncState(*file)) {
        if (!_planDelayedFolderUpdate(fileCloudDir, updateTimeoutMs))
            delete file;
            return;    // the chain for this file was cut
    } else {
        delete file;
    }
}


bool CloudMailRuExtension::_isOneOfCloudHiddenSystemFiles(const string &cloudFilePath)
{
    return cloudFilePath == "/.cloud" || cloudFilePath == "/.cloud_ss";
}


void CloudMailRuExtension::_writeDefaultConfig()
{
    _config.add("User.logged_in", "false");

    _config.add("Emblems.sync_completed", "stock_calc-accept");
    _config.add("Emblems.sync_in_progress", "stock_refresh");
    _config.add("Emblems.sync_shared", "applications-roleplaying");

    _config.add("SyncTweaks.in_progress_update_interval", 4000);
    _config.add("SyncTweaks.min_update_interval", 2000);
    _config.add("SyncTweaks.max_active_folders", 3);
    _config.add("SyncTweaks.max_file_updates", 10);
}


void CloudMailRuExtension::_ensureCloudAPIIsReady()
{
    if (_cloudAPI.loggedIn())
        return;    // everything is allright

    if (_config.get<bool>("User.logged_in")) {
        if (_cloudAPI.loginWithCookies(_config.get_child("ApiSession"))) {
            std::cout << extension_info::logPrefix << "logged in successfully using stored cookies" << std::endl;
            return;
        } else {
            std::cout << extension_info::logPrefix << "warning: login attempt using stored cookies failed" << std::endl;
        }
    }

    do {
        string password = _gui->showModalAuthDialog(_mailRuUserName);

        std::cout << extension_info::logPrefix << "logging into mail.ru cloud rest api ...    " << std::endl;
        if (_cloudAPI.login(_mailRuUserName, password)) {

            std::cout << extension_info::logPrefix << "logged in successfully" << std::endl;
            _config.put_child("ApiSession", b_pt::ptree());
            _cloudAPI.storeCookies(_config.get_child("ApiSession"));
            _config.put("User.logged_in", "true");
            _saveConfiguration();
            break;
        }

        if (password == "None" || password == "") {    // this means the user refused password entry
            _running = false;
            break;
        }

    } while (!_cloudAPI.loggedIn());
}


bool CloudMailRuExtension::_planDelayedFolderUpdate(const string &dirName, int updateTimeoutMs)
{
    string taskId = dirName + "#delayed";

    if (_updateActiveFolders.find(dirName) == _updateActiveFolders.end()) {
        return false;
    }

    if (!_isTaskOnQueue(taskId)) {
        _enqueueAsyncTask(taskId, system_clock::now() + milliseconds(updateTimeoutMs),
                          std::bind(&CloudMailRuExtension::_folderUpdateTask, this, dirName));
    }

    return true;
}
