
#ifndef NAUTILUS_MAILRU_CLOUD_EXTENSIONERROR_HPP
#define NAUTILUS_MAILRU_CLOUD_EXTENSIONERROR_HPP

//----------------------------------------------------------------------------------------------------------------------

#include <system_error>
#include <sstream>
#include <exception>
using std::string;

//----------------------------------------------------------------------------------------------------------------------

class ExtensionError : public std::exception
{
public:
    enum code_t { CLIENT_CONFIG_ERROR, CONFIG_ERROR, SYSCALL_FAILED };

private:
    code_t _code;
    string _reason;

public:
    ExtensionError(code_t code, string reason = "") : _code(code), _reason(reason)  { }

    ExtensionError(string failedSyscall) : _code(SYSCALL_FAILED) {
        std::ostringstream msg;
        msg << "syscall failed: " << failedSyscall << "\t [ errno = " << errno << ": ";
        msg << std::system_error(errno, std::system_category()).what() << " ]";

        _reason = msg.str();
    }

    inline code_t code() const noexcept {
        return _code;
    }

    virtual const char* what() const noexcept {
        return _reason.c_str();
    }
};

//----------------------------------------------------------------------------------------------------------------------

#define syscall_check(result)    if ((result) < 0)  throw ExtensionError(#result)

//----------------------------------------------------------------------------------------------------------------------

#endif    //NAUTILUS_MAILRU_CLOUD_EXTENSIONERROR_HPP
