
#include "GUIProvider.hpp"
#include "AuthDialogGtk.hpp"
#include "ExtensionError.hpp"

#include <libnotify/notify.h>
#include <sys/wait.h>
#include <iostream>

//----------------------------------------------------------------------------------------------------------------------

void GUIProviderGtk::showCopyPublicLinkNotification(const string &msgText)
{
   // auto routine = []() -> gboolean {
        NotifyNotification *notification = notify_notification_new("Nautilus Cloud@Mail.Ru",
                                                                   msgText.c_str(),
                                                                   "/usr/share/icons/hicolor/256x256/apps/mail.ru-cloud.png");
        notify_notification_show(notification, nullptr);
        g_object_unref(G_OBJECT(notification));

    //    return true;
    //};

    //g_main_context_invoke(nullptr, routine);
}


void GUIProviderGtk::copyToClipboard(const string &text)
{
    gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), text.c_str(), (gint)text.length());
}


GUIProviderGtk::GUIProviderGtk()
{
    notify_init("Nautilus Cloud@Mail.Ru");
}


GUIProviderGtk::~GUIProviderGtk()
{
    notify_uninit();
}


void GUIProviderGtk::invokeInGUIThread(function<void()> routine)
{
    auto callback = new function<void()>();
    *callback = routine;

    g_main_context_invoke(nullptr, &GUIProviderGtk::_guiThreadInvoker, (gpointer)callback);
}


gboolean GUIProviderGtk::_guiThreadInvoker(gpointer dataPtr)
{
    auto callback = (function<void()> *)dataPtr;
    (*callback)();
    delete callback;

    return false;
}


string GUIProviderGtk::showModalAuthDialog(const string &forLogin)
{
    auto externalGUI = _invokeExternalPythonGUI("auth_dialog");

    string password;
    password.reserve(1000);  // TODO: fix it if it works
    fscanf(externalGUI.commFrom, "%s", password.c_str());
    std::cin >> password;

    if (password.back() == '\n') {
        password.erase(password.begin()-1);
    }

    _waitForExternalPythonGUIExit(externalGUI);
    return password;
}


auto GUIProviderGtk::_invokeExternalPythonGUI(const string &componentName) -> _ExternalGUITask
{
    int communicationPipeFrom[2], communicationPipeTo[2];
    syscall_check( ::pipe(communicationPipeFrom) );
    syscall_check( ::pipe(communicationPipeTo) );

    pid_t childPID = 0;
    if ((childPID = ::fork()) == 0) {
        syscall_check( ::dup2(communicationPipeFrom[1], ::fileno(stdout)) );
        syscall_check( ::dup2(communicationPipeTo[0],   ::fileno(stdin)) );

        ::close(communicationPipeFrom[0]);
        ::close(communicationPipeTo[1]);

        int execResult = ::execlp("python", "python",
                 _externalPythonGUIFile.c_str(), componentName.c_str(), nullptr);
        syscall_check( execResult );
    }
    syscall_check( childPID );

    ::close(communicationPipeFrom[1]);
    ::close(communicationPipeTo[0]);

    _ExternalGUITask guiTask;
    guiTask.pid = childPID;
    guiTask.commFrom = ::fdopen(communicationPipeFrom[0], "r");
    guiTask.commTo = ::fdopen(communicationPipeTo[1], "w");

    return guiTask;
}


void GUIProviderGtk::_waitForExternalPythonGUIExit(_ExternalGUITask &task)
{
    int exitState = 0;
    ::waitpid(task.pid, &exitState, 0);

    fclose(task.commFrom);
    fclose(task.commTo);
}
