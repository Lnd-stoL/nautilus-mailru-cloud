
#ifndef NAUTILUS_MAILRU_CLOUD_GUIPROVIDER_H
#define NAUTILUS_MAILRU_CLOUD_GUIPROVIDER_H

//----------------------------------------------------------------------------------------------------------------------

#include <gtk/gtk.h>

#include <functional>
#include <string>

using std::string;
using std::function;

#include "extension_info.hpp"

//----------------------------------------------------------------------------------------------------------------------

class GUIProvider
{
public:
    virtual string showModalAuthDialog(const string &forLogin) = 0;
    virtual void showCopyPublicLinkNotification(const string &msgText) = 0;
    virtual void copyToClipboard(const string &text) = 0;
    virtual void invokeInGUIThread(function<void()> routine) = 0;

    virtual ~GUIProvider() { };
};

//----------------------------------------------------------------------------------------------------------------------

class GUIProviderGtk : public GUIProvider
{
private:
    const string _externalPythonGUIFile =
            string(extension_info::extensionLoadPath) + "/nautilus_mailru_cloud_gtk_gui_services.py";

    struct _ExternalGUITask
    {
        pid_t pid = 0;
        FILE* commFrom = nullptr;
        FILE* commTo = nullptr;
    };


private:
    static gboolean _guiThreadInvoker(gpointer dataPtr);
    _ExternalGUITask _invokeExternalPythonGUI(const string& componentName);
    void _waitForExternalPythonGUIExit(_ExternalGUITask& guiTask);

public:
    GUIProviderGtk();
    virtual ~GUIProviderGtk();

public:
    virtual string showModalAuthDialog(const string &forLogin);
    virtual void showCopyPublicLinkNotification(const string &msgText);
    virtual void copyToClipboard(const string &text);
    virtual void invokeInGUIThread(function<void()> routine);
};

//----------------------------------------------------------------------------------------------------------------------

#endif    //NAUTILUS_MAILRU_CLOUD_GUIPROVIDER_H
