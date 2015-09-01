
#include "CloudMailRuExtension.hpp"
#include "extension_info.hpp"
#include "GUIProvider.hpp"

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
        std::cout << extension_info::logPrefix << "logging into mail.ru ...    " << std::endl;
        _cloudAPI.login(_mailRuUserName, _config.get<string>("User.password"));
        std::cout << extension_info::logPrefix << "logged in" << std::endl;

        while (_running) {
            std::unique_lock<std::mutex> lock(_tasksAccessLocker);
            _tasksUpdateTrigger.wait(lock);

            while (!_asyncTasksQueue.empty()) {
                auto nextTaskIt = _asyncTasksQueue.begin();

                lock.unlock();
                nextTaskIt->task();
                lock.lock();

                _asyncTasksQueue.pop_front();
            }
        }
    }).detach();
}


void CloudMailRuExtension::getContextMenuItemsForFile(const FileInfo &file, vector<FileContextMenuItem> &items)
{
    if (file.path().find(_localCloudDir) != 0)
        return;    // no items outside cloud dir

    string fileName = file.pathInfo().filename().string();
    string fileCloudPath = _cloudPath(file);

    static const string publicLinkText = "Get public link to \"";
    items.emplace_back(publicLinkText + fileName + '"', "GetPublicLink");

    auto asyncTask =  [this, fileCloudPath, fileName]() {
        string link = _cloudAPI.getPublicLinkTo(fileCloudPath);
        std::cout << extension_info::logPrefix << "got publink link to " << fileCloudPath << " : " << link << std::endl;

        _gui->copyToClipboard(link);
        _gui->showCopyPublicLinkNotification(string("Publink link to '") + fileName + "' copied to clipboard.");
    };

    items.back().onClick(
        [this, fileCloudPath, asyncTask]() {
            _enqueueAsyncTask(fileCloudPath + "#weblink", asyncTask);
        });
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
        throw Error(Error::CLIENT_CONFIG_ERROR, string("exception thrown: ") + parserErr.what());
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

    if (!_isTaskOnQueue(fileCloudDir)) {    // this is to prevent directory net update for each file in a folder
        _enqueueAsyncTask(fileCloudDir, [this, fileCloudDir]() {
            _netUpdateDirectory(fileCloudDir);
        });
    }

    _enqueueAsyncTask(fileCloudDir, [this, file]() {
        _updateFileSyncState(*file);
        delete file;
    });
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


void CloudMailRuExtension::_netUpdateDirectory(const string &dirName)
{
    std::cout << extension_info::logPrefix << "started getting direcory items for " << dirName << std::endl;

    vector<CloudMailRuRestAPI::CloudFileInfo> dirItems;
    _cloudAPI.getFolderContents(dirName, dirItems);

    for (auto &nextItem : dirItems) {
        _cachedCloudFiles[nextItem.path] = nextItem;
    }

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
        return true;

    } else {
        if (std::abs(b_fs::last_write_time(file.pathInfo()) - cachedFileInfo->second.mtime) < 1) {
            file.setSyncState(FileInfo::ACTUAL);
            return false;
        } else {
            file.setSyncState(FileInfo::IN_PROGRESS);
            return true;
        }
    }
}


void CloudMailRuExtension::_enqueueAsyncTask(const string &id, function<void()> task)
{
    _tasksAccessLocker.lock();

    _AsyncTask newTask = {};
    newTask.id = id;
    newTask.task = task;
    _asyncTasksQueue.push_back(std::move(newTask));

    _tasksAccessLocker.unlock();
    _tasksUpdateTrigger.notify_all();
}


bool CloudMailRuExtension::_readConfiguration()
{
    if (!b_fs::exists(_configFileName())) {
        //TODO: configure

        _saveConfiguration();
    }

    try {
        b_pt::read_ini(_configFileName(), _config);
    }
    catch (const std::exception &parserErr) {
        std::cout << extension_info::logPrefix << "error while reading config: " << parserErr.what() << std::endl;
        throw Error(Error::CONFIG_ERROR, string("exception thrown: ") + parserErr.what());
    }

    return true;
}


void CloudMailRuExtension::_saveConfiguration()
{
    b_pt::write_ini(_configFileName(), _config);
}


string CloudMailRuExtension::_configFileName()
{
    return (_userHomeDirPath() / ".config/Mail.Ru/Mail.Ru_Cloud-NautilusExtension.conf").string();
}


bool CloudMailRuExtension::_isTaskOnQueue(const string &id)
{
    std::unique_lock<std::mutex> lock(_tasksAccessLocker);

    _AsyncTask fakeTask;
    fakeTask.id = id;

    return std::find(_asyncTasksQueue.begin(), _asyncTasksQueue.end(), fakeTask) != _asyncTasksQueue.end();
}


CloudMailRuExtension::~CloudMailRuExtension()
{
    _running = false;
    _tasksUpdateTrigger.notify_all();
}
