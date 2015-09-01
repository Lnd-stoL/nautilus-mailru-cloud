
#include "GUIProvider.hpp"

#include <gtk/gtk.h>
#include <libnotify/notify.h>

//----------------------------------------------------------------------------------------------------------------------

void GUIProviderGtk::showCopyPublicLinkNotification(const string &msgText)
{
    NotifyNotification *notification = notify_notification_new("Nautilus Cloud@Mail.Ru",
                                                               msgText.c_str(),
                                                               "/usr/share/icons/hicolor/256x256/apps/mail.ru-cloud.png");
    notify_notification_show(notification, nullptr);
    g_object_unref(G_OBJECT(notification));

    /*
    if (::fork() == 0) {
        ::execlp("notify-send", "notify-send", "-i", "/usr/share/icons/hicolor/256x256/apps/mail.ru-cloud.png",
                 "Nautilus Cloud@Mail.Ru", (string("\"") + msgText + "\"").c_str(), nullptr);
    }
     */
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
