
#ifndef NAUTILUS_MAILRU_CLOUD_ABSTRACTMENUITEM_H
#define NAUTILUS_MAILRU_CLOUD_ABSTRACTMENUITEM_H

//----------------------------------------------------------------------------------------------------------------------

#include "FileInfo.hpp"

#include <functional>
using std::function;

//----------------------------------------------------------------------------------------------------------------------

class MenuItem
{
private:
    string _title;
    string _name;
    string _description;
    string _icon;


public:
    MenuItem(const string &&title, const string&& name, const string &&icon = "", const string && desc = "") :
            _title(title), _name(name), _description(desc), _icon(icon)
    { }

public:
    inline const string& title() const {
        return _title;
    }

    inline const string& name() const {
        return _name;
    }

    inline const string& description() const {
        return _description;
    }

    inline const string& icon() const {
        return _icon;
    }
};

//----------------------------------------------------------------------------------------------------------------------

class FileContextMenuItem : public MenuItem
{
public:
    typedef function<void()> click_handler_t;

private:
    click_handler_t  _handler;


public:
    FileContextMenuItem(const string &&title, const string&& name, const string &&icon = "", const string &&desc = "") :
            MenuItem(std::move(title), std::move(name), std::move(icon), std::move(desc))
    { }

    void onClick(click_handler_t handler) {
        _handler = handler;
    }

public:
    inline const click_handler_t clickHandler() const {
        return _handler;
    }
};

//----------------------------------------------------------------------------------------------------------------------

#endif    //NAUTILUS_MAILRU_CLOUD_ABSTRACTMENUITEM_H
