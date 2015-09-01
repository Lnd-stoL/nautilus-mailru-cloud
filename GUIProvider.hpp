
#ifndef NAUTILUS_MAILRU_CLOUD_GUIPROVIDER_H
#define NAUTILUS_MAILRU_CLOUD_GUIPROVIDER_H

//----------------------------------------------------------------------------------------------------------------------

#include <string>
using std::string;

//----------------------------------------------------------------------------------------------------------------------

class GUIProvider
{
public:
    virtual void showCopyPublicLinkNotification(const string &msgText) = 0;
    virtual void copyToClipboard(const string &text) = 0;
};

//----------------------------------------------------------------------------------------------------------------------

class GUIProviderGtk : public GUIProvider
{
public:
    GUIProviderGtk();
    ~GUIProviderGtk();

public:
    virtual void showCopyPublicLinkNotification(const string &msgText);
    virtual void copyToClipboard(const string &text);
};

//----------------------------------------------------------------------------------------------------------------------

#endif    //NAUTILUS_MAILRU_CLOUD_GUIPROVIDER_H
