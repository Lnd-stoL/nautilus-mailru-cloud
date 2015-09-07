
# -*- coding: utf-8 -*-
__version__ = "0.1 preview"

#-----------------------------------------------------------------------------------------------------------------------
# tunables

CONFIG_PATH = '/.config/Mail.Ru/Mail.Ru_Cloud-NautilusExtension.conf'    # relative to home dir


#-----------------------------------------------------------------------------------------------------------------------
# imports

from gi.repository import GObject, Gdk, GdkPixbuf, Gtk, Gio, GLib
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
        self.set_default_size(500, 200)
        self.set_icon_from_file("/usr/share/icons/hicolor/256x256/apps/mail.ru-cloud.png")
        
        top_hbox = Gtk.HBox(homogeneous=False, spacing=0)
        
        # Mail.Ru logo icon
        icon_frame = Gtk.Frame()
        icon_frame.set_shadow_type(Gtk.ShadowType.IN)
        icon_align = Gtk.Alignment(xalign=0.5, yalign=0.5, xscale=0, yscale=0)
        icon_align.add(icon_frame)
        icon_pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_size("/usr/share/icons/hicolor/256x256/apps/mail.ru-cloud.png", 192, 192)
        icon_transparent = icon_pixbuf.add_alpha(True, 0xff, 0xff, 0xff)
        icon_image = Gtk.Image.new_from_pixbuf(icon_transparent)
        icon_frame.add(icon_image)

        top_hbox.pack_start(icon_align, False, False, 10)
        right_vbox = Gtk.VBox()
        top_hbox.pack_end(right_vbox, False, False, 10)
        
        label_top = Gtk.Label()
        label_top.set_markup("Для работы плагина <b>nautilus-cloud@mail.ru</b> необходимо войти в облако.")
        label_top.set_line_wrap(True)
        right_vbox.pack_start(label_top, True, True, 5)

        label = Gtk.Label()
        label.set_markup("<b>Введите пароль:</b>")
        right_vbox.pack_start(label, True, True, 10)
        
        entry = Gtk.Entry()
        entry.set_visibility(False)
        entry.set_text(str(default_value))
        entry.connect("activate",
                      lambda ent, dlg, resp: dlg.response(resp),
                      self, Gtk.ResponseType.OK)
        
        entry_box = Gtk.HBox(homogeneous=False, spacing=0)
        entry_box.pack_start(entry, True, True, 20)
        right_vbox.pack_end(entry_box, False, False, 5)
        
        self.vbox.pack_start(top_hbox, False, False, 10)
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

