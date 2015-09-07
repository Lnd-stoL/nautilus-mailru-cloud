
#include "GUIProvider.hpp"
#include "AuthDialogGtk.hpp"

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


string GUIProviderGtk::showModalAuthDialog()
{
    _invokeExternalPythonGUI("auth_dialog");

    string password;
    std::cin >> password;

    if (password.back() == '\n') {
        password.erase(password.begin()-1);
    }

    return password;

    //this->invokeInGUIThread([]() {
        //AuthDialogGtk dlg;
        //dlg.show_all();
    //});
}


void GUIProviderGtk::_invokeExternalPythonGUI(const string &componentName)
{
    int communicationPipe[2];
    ::pipe(communicationPipe);

    pid_t childPID = 0;
    if ((childPID = ::fork()) == 0) {
        ::dup2(communicationPipe[1], ::fileno(stdout));

        ::execlp("python", "python",
                 _externalPythonGUIFile.c_str(), componentName.c_str(), nullptr);
    }

    ::dup2(communicationPipe[0], ::fileno(stdin));

    int exitState = 0;
    ::waitpid(childPID, &exitState, 0);
}
