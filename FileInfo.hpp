
#ifndef NAUTILUS_MAILRU_CLOUD_FILEINFO_HPP
#define NAUTILUS_MAILRU_CLOUD_FILEINFO_HPP

//----------------------------------------------------------------------------------------------------------------------

#include <string>
using std::string;

#include <boost/filesystem.hpp>
namespace b_fs = boost::filesystem;

//----------------------------------------------------------------------------------------------------------------------

class FileInfo
{
public:
    enum sync_state_t {
        UNKNOWN,
        IN_PROGRESS,
        ACTUAL,
        ACTUAL_SHARED
    };

protected:
    sync_state_t _syncState = UNKNOWN;


public:
    virtual string uri()  const = 0;
    virtual string path() const = 0;

    virtual bool isStillActual() = 0;
    virtual void invalidateExtensionInfo() = 0;

    virtual ~FileInfo() { }


public:
    inline b_fs::path pathInfo() const {
        return b_fs::path(this->path());
    }

    // returns true if sync state changed
    virtual bool setSyncState(sync_state_t state) {
        bool ret = _syncState != state;
        _syncState = state;
        return ret;
    };


public:
    inline sync_state_t syncState() const {
        return _syncState;
    }
};

//----------------------------------------------------------------------------------------------------------------------

#endif //NAUTILUS_MAILRU_CLOUD_FILEINFO_HPP
