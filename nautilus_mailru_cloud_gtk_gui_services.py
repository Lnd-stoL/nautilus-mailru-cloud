
# -*- coding: utf-8 -*-
__version__ = "0.1 preview"

#-----------------------------------------------------------------------------------------------------------------------
# tunables

CONFIG_PATH = '/.config/Mail.Ru/Mail.Ru_Cloud-NautilusExtension.conf'    # relative to home dir


#-----------------------------------------------------------------------------------------------------------------------
# imports

from gi.repository import GObject, Gtk, Gio, GLib
import os.path as os_path

import sys
import ConfigParser


#-----------------------------------------------------------------------------------------------------------------------
# GUI components

class PasswordEntryDialog(Gtk.Dialog):

    def __init__(self, *args, **kwargs):
        if 'default_value' in kwargs:
            default_value = kwargs['default_value']
            del kwargs['default_value']
        else:
            default_value = ''

        dlg_buttons = (Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL, Gtk.STOCK_OK, Gtk.ResponseType.OK)
        super(PasswordEntryDialog, self).__init__(title='Авторизация в Облаке Mail.Ru', flags=0, buttons=dlg_buttons, *args, **kwargs)
        self.set_default_size(200, 150)

        label = Gtk.Label()
        label.set_markup("<b>Введите пароль: </b>")
        self.vbox.pack_start(label, True, True, 5)

        entry = Gtk.Entry()
        entry.set_visibility(False)
        entry.set_text(str(default_value))
        entry.connect("activate",
                      lambda ent, dlg, resp: dlg.response(resp),
                      self, Gtk.ResponseType.OK)
        self.vbox.pack_end(entry, True, True, 10)
        self.vbox.show_all()
        self.entry = entry

    def set_value(self, text):
        self.entry.set_text(text)

    def run(self):
        result = super(PasswordEntryDialog, self).run()
        if result == Gtk.ResponseType.OK:
            text = self.entry.get_text()
        else:
            text = None
        return text
        
#-----------------------------------------------------------------------------------------------------------------------

class ErrorDialog(Gtk.MessageDialog):
     def __init__(self, message):
        super(ErrorDialog, self).__init__(None, 0, Gtk.MESSAGE_INFO, Gtk.BUTTONS_CLOSE, message)
        self.set_default_size(200, 150)

     def show(self):
         self.run()
         self.destroy()

#-----------------------------------------------------------------------------------------------------------------------
# entry point

for arg in sys.argv:
    if arg == "auth_dialog":
        dlg = PasswordEntryDialog()
        password_str = dlg.run()
        dlg.destroy()
        print(password_str)

