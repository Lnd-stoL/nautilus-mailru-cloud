
#include "AuthDialogGtk.hpp"
#include <iostream>

//----------------------------------------------------------------------------------------------------------------------

extern "C"
void _loginClicked(GtkWidget *widget, gpointer data);


AuthDialogGtk::AuthDialogGtk()
{
    _window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title(GTK_WINDOW(_window), "Облако Mail.Ru - Авторизация");
    gtk_window_set_default_geometry(GTK_WINDOW(_window), 500, 400);

    GtkWidget *btn = gtk_button_new();
    gtk_button_set_label(GTK_BUTTON(btn), "Войти");
    gtk_container_add(GTK_CONTAINER(_window), btn);

    g_signal_connect(G_OBJECT(btn), "clicked",
                    G_CALLBACK(&_loginClicked), (gpointer)this);

    gtk_widget_show_all(_window);
}


void _loginClicked(GtkWidget *widget, gpointer data)
{
    AuthDialogGtk *self = (AuthDialogGtk *)data;
    gtk_window_close(GTK_WINDOW(self->_window));
}
