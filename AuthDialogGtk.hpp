
#ifndef NAUTILUS_MAILRU_CLOUD_AUTHDIALOGGTK_HPP
#define NAUTILUS_MAILRU_CLOUD_AUTHDIALOGGTK_HPP

//----------------------------------------------------------------------------------------------------------------------

#include <gtk/gtk.h>
#include <glib.h>

//----------------------------------------------------------------------------------------------------------------------

class AuthDialogGtk
{
public:
    GtkWidget *_window = nullptr;

private:
    //extern "C" {
    //}

public:
    AuthDialogGtk();
};

//----------------------------------------------------------------------------------------------------------------------

#endif    //NAUTILUS_MAILRU_CLOUD_AUTHDIALOGGTK_HPP
