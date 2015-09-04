
#ifndef NAUTILUS_MAILRU_CLOUD_GUIPROVIDER_H
#define NAUTILUS_MAILRU_CLOUD_GUIPROVIDER_H

//----------------------------------------------------------------------------------------------------------------------

#include <gtk/gtk.h>

#include <functional>
#include <string>

using std::string;
using std::function;

//----------------------------------------------------------------------------------------------------------------------

class GUIProvider
{
public:
    virtual void showCopyPublicLinkNotification(const string &msgText) = 0;
    virtual void copyToClipboard(const string &text) = 0;
    virtual void invokeInGUIThread(function<void()> routine) = 0;

    virtual ~GUIProvider() { };
};

//----------------------------------------------------------------------------------------------------------------------

class GUIProviderGtk : public GUIProvider
{
private:
    static gboolean _guiThreadInvoker(gpointer dataPtr);

public:
    GUIProviderGtk();
    virtual ~GUIProviderGtk();

public:
    virtual void showCopyPublicLinkNotification(const string &msgText);
    virtual void copyToClipboard(const string &text);
    virtual void invokeInGUIThread(function<void()> routine);
};

//----------------------------------------------------------------------------------------------------------------------

#endif    //NAUTILUS_MAILRU_CLOUD_GUIPROVIDER_H
