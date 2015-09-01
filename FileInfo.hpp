
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
    enum sync_state { UNKNOWN, IN_PROGRESS, ACTUAL };

public:
    virtual string uri()  const = 0;
    virtual string path() const = 0;

    virtual void setSyncState(sync_state state) = 0;


public:
    inline b_fs::path pathInfo() const {
        return b_fs::path(this->path());
    }
};

//----------------------------------------------------------------------------------------------------------------------

#endif //NAUTILUS_MAILRU_CLOUD_FILEINFO_HPP
