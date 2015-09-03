
#ifndef NAUTILUS_MAILRU_CLOUD_URLTOOLS_HPP
#define NAUTILUS_MAILRU_CLOUD_URLTOOLS_HPP

//----------------------------------------------------------------------------------------------------------------------

#include <string>
using std::string;

//----------------------------------------------------------------------------------------------------------------------

namespace URLTools
{
    string encode(const string &src);
    string decode(const string &src);
};

//----------------------------------------------------------------------------------------------------------------------

#endif    //NAUTILUS_MAILRU_CLOUD_URLTOOLS_HPP
