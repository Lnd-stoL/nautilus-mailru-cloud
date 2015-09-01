
#include <iostream>

#include <libnautilus-extension/nautilus-column-provider.h>
#include <libnautilus-extension/nautilus-info-provider.h>
#include <libnautilus-extension/nautilus-menu-provider.h>

#include "extension_info.hpp"
#include "CloudMailRuExtension.hpp"

//----------------------------------------------------------------------------------------------------------------------

namespace nautilus_extension
{
    static GType extensionExportedType;


    struct CloudMailRuExtension_Glue
    {
        GObject parentSlot;

        CloudMailRuExtension *extension = nullptr;
        GUIProvider *guiProvider = nullptr;
    };


    struct CloudMailRuExtension_ClassGlue
    {
        GObjectClass parentSlot;
    };


    class NautilusFileInfo_Glue : public FileInfo
    {
    private:
        NautilusFileInfo *_fileInfo;

        mutable string _uri;    // they are mutable because if caching effect of getters
        mutable string _path;

    private:
        string _fastUriDecode(const string &sSrc) const;

    public:
        NautilusFileInfo_Glue(NautilusFileInfo *fileInfo) : _fileInfo(fileInfo) {
            g_object_ref(_fileInfo);
        }

        NautilusFileInfo_Glue(const NautilusFileInfo_Glue &from) {
            _uri = from._uri;
            _path = from._path;
            _fileInfo = from._fileInfo;
            g_object_ref(_fileInfo);
        }

        ~NautilusFileInfo_Glue() {
            g_object_unref(_fileInfo);
        }


        inline virtual string uri() const {
            if (_uri.empty()) {
                char *uriPtr = nautilus_file_info_get_uri(_fileInfo);
                _uri = uriPtr;
                g_free(uriPtr);
            }
            return _uri;
        }

        inline virtual string path() const {
            if (_path.empty()) {
                _path = _fastUriDecode(uri());
                auto pathSubstr = _path.substr(_path.find(':') + 3);
                _path.swap(pathSubstr);
            }
            return _path;
        }


        inline virtual void setSyncState(sync_state state) {
            if (state == FileInfo::sync_state::ACTUAL) {
                nautilus_file_info_add_emblem(_fileInfo, "stock_calc-accept");
            }
            if (state == FileInfo::sync_state::IN_PROGRESS) {
                nautilus_file_info_add_emblem(_fileInfo, "stock_refresh");
            }
        }
    };


    struct FileInfoUpdateHandlerData
    {
        GClosure *update_complete;
        NautilusInfoProvider *provider;
        NautilusFileInfo *file;
        int operation_handle;
        gboolean cancelled;
    };


    static void classInit(CloudMailRuExtension_ClassGlue *extensionClass);
    static void instanceInit(CloudMailRuExtension_Glue *extensionInstance);
    static void instanceFinalize(GObject *extensionInstance);

    static void menuProviderIfaceInit(NautilusMenuProviderIface *iface);
    static void infoProviderIfaceInit(NautilusInfoProviderIface *iface);
    static GList*getFilesMenuItems(NautilusMenuProvider *provider, GtkWidget *window, GList *files);
    static void onMenuItemClick(NautilusMenuItem *item, gpointer user_data);

    static NautilusOperationResult updateFileInfo(NautilusInfoProvider *provider, NautilusFileInfo *file,
                                                GClosure *update_complete, NautilusOperationHandle **handle);
    //static void file_data_ready_cb(char *data, FileInfoUpdateHandlerData *handlerData);
}


//----------------------------------------------------------------------------------------------------------------------
// module-level nautilus extension interface
//----------------------------------------------------------------------------------------------------------------------

extern "C"
void nautilus_module_initialize(GTypeModule  *module)
{
    GTypeInfo mainExtensionTypeInfo = {};
    mainExtensionTypeInfo.class_size    = sizeof(nautilus_extension::CloudMailRuExtension_ClassGlue);
    mainExtensionTypeInfo.class_init    = (GClassInitFunc) nautilus_extension::classInit;
    mainExtensionTypeInfo.instance_size = sizeof(nautilus_extension::CloudMailRuExtension_Glue);
    mainExtensionTypeInfo.instance_init = (GInstanceInitFunc) nautilus_extension::instanceInit;

    nautilus_extension::extensionExportedType =
            g_type_module_register_type(module, G_TYPE_OBJECT, "MailRuCloudExtension", &mainExtensionTypeInfo,
                                        (GTypeFlags)0);

    GInterfaceInfo menuProviderIfaceInfo = {};
    menuProviderIfaceInfo.interface_init = (GInterfaceInitFunc) nautilus_extension::menuProviderIfaceInit;
    g_type_module_add_interface(module, nautilus_extension::extensionExportedType, NAUTILUS_TYPE_MENU_PROVIDER,
                                &menuProviderIfaceInfo);

    GInterfaceInfo fileInfoProviderIfaceInfo = {};
    fileInfoProviderIfaceInfo.interface_init = (GInterfaceInitFunc) nautilus_extension::infoProviderIfaceInit;
    g_type_module_add_interface(module, nautilus_extension::extensionExportedType, NAUTILUS_TYPE_INFO_PROVIDER,
                                &fileInfoProviderIfaceInfo);

    std::cout << extension_info::logPrefix << "extension initialized; ver. " << extension_info::versionString
                << "; by " << extension_info::authors << std::endl;
}


extern "C"
void nautilus_module_shutdown()
{
    std::cout << extension_info::logPrefix << "extension shutting down" << std::endl;
}


extern "C"
void nautilus_module_list_types(const GType **types, int *num_types)
{
    *types = &nautilus_extension::extensionExportedType;
    *num_types = 1;
}


//----------------------------------------------------------------------------------------------------------------------
// menu provider interface
//----------------------------------------------------------------------------------------------------------------------

void nautilus_extension::classInit(CloudMailRuExtension_ClassGlue *extensionClass)
{
    extensionClass->parentSlot.dispose = (nautilus_extension::instanceFinalize);
}


void nautilus_extension::instanceInit(CloudMailRuExtension_Glue *extensionInstance)
{
    extensionInstance->extension = nullptr;
    extensionInstance->guiProvider = nullptr;

    try {
        extensionInstance->guiProvider = new GUIProviderGtk();
        extensionInstance->extension = new CloudMailRuExtension(extensionInstance->guiProvider);
    }
    catch(CloudMailRuExtension::Error extensionErr) {
        std::cout << "!" << std::endl;
        extensionInstance->extension = nullptr;
    }
    catch(std::exception err) {
        std::cout << "!" << std::endl;
        extensionInstance->extension = nullptr;
    }
}


void nautilus_extension::instanceFinalize(GObject *extensionInstanceObj)
{
    nautilus_extension::CloudMailRuExtension_Glue *extensionInstance
            = (CloudMailRuExtension_Glue *)extensionInstanceObj;

    delete extensionInstance->extension;
    delete extensionInstance->guiProvider;
}


void nautilus_extension::menuProviderIfaceInit(NautilusMenuProviderIface *iface)
{
    iface->get_file_items = nautilus_extension::getFilesMenuItems;
}


void nautilus_extension::infoProviderIfaceInit(NautilusInfoProviderIface *iface)
{
    iface->update_file_info = nautilus_extension::updateFileInfo;
}


GList* nautilus_extension::getFilesMenuItems(NautilusMenuProvider *provider, GtkWidget *window, GList *files)
{
    if (files == nullptr || files->next != nullptr)
        return nullptr;   // we handle only exactly one file

    CloudMailRuExtension* extension = ((CloudMailRuExtension_Glue *)provider)->extension;
    if (extension == nullptr)  return nullptr;   // no valid initialized extension class

    vector<FileContextMenuItem> items;
    extension->getContextMenuItemsForFile(NautilusFileInfo_Glue(NAUTILUS_FILE_INFO(files->data)), items);
    if (items.empty())  return nullptr;   // no menu items to add

    NautilusMenuItem *mailruSubmenuItem = nautilus_menu_item_new("CloudMailRuExtension::MailRuSubmenu", "Cloud@Mail.Ru",
                                                    "Cloud@Mail.Ru related actions", nullptr);
    NautilusMenu *mailruSubmenu = nautilus_menu_new();
    nautilus_menu_item_set_submenu(mailruSubmenuItem, mailruSubmenu);
    GList *itemsList = g_list_append(nullptr, mailruSubmenuItem);

    for (auto const & absItem : items) {
        static const string itemIdStrPrefix = "CloudMailRuExtension::";
        NautilusMenuItem *item = nautilus_menu_item_new((itemIdStrPrefix + absItem.name()).c_str(), absItem.title().c_str(),
                                      absItem.description().c_str(), nullptr);

        g_signal_connect(item, "activate", G_CALLBACK(nautilus_extension::onMenuItemClick), provider);
        g_object_set_data_full((GObject *)item, "files_list",
                                nautilus_file_info_list_copy(files),
                                (GDestroyNotify)nautilus_file_info_list_free);

        auto handler = new FileContextMenuItem::click_handler_t(absItem.clickHandler());
        g_object_set_data((GObject *)item, "click_handler", handler);
        nautilus_menu_append_item(mailruSubmenu, item);
    }

    return itemsList;
}


void nautilus_extension::onMenuItemClick(NautilusMenuItem *item, gpointer user_data)
{
    GList *files = (GList *)g_object_get_data((GObject *)item, "files_list");
    auto *handler = (FileContextMenuItem::click_handler_t *)g_object_get_data((GObject *)item, "click_handler");

    NautilusFileInfo_Glue fileInfo(NAUTILUS_FILE_INFO(files->data));
    (*handler)(fileInfo);
    delete handler;
}


string nautilus_extension::NautilusFileInfo_Glue::_fastUriDecode(const string &sSrc) const
{
    static const char HEX2DEC[256] =
    {
        /*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
        /* 0 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 1 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 2 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,

        /* 4 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 5 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 6 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 7 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

        /* 8 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* 9 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* A */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* B */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

        /* C */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* D */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* E */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        /* F */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
    };

    // Note from RFC1630: "Sequences which start with a percent
    // sign but are not followed by two hexadecimal characters
    // (0-9, A-F) are reserved for future extension"

    const unsigned char * pSrc = (const unsigned char *)sSrc.c_str();
    const int SRC_LEN = sSrc.length();
    const unsigned char * const SRC_END = pSrc + SRC_LEN;
    // last decodable '%'
    const unsigned char * const SRC_LAST_DEC = SRC_END - 2;

    char * const pStart = new char[SRC_LEN];
    char * pEnd = pStart;

    while (pSrc < SRC_LAST_DEC)
    {
        if (*pSrc == '%')
        {
            char dec1, dec2;
            if (-1 != (dec1 = HEX2DEC[*(pSrc + 1)])
                && -1 != (dec2 = HEX2DEC[*(pSrc + 2)]))
            {
                *pEnd++ = (dec1 << 4) + dec2;
                pSrc += 3;
                continue;
            }
        }

        *pEnd++ = *pSrc++;
    }

    // the last 2- chars
    while (pSrc < SRC_END)
        *pEnd++ = *pSrc++;

    std::string sResult(pStart, pEnd);
    delete [] pStart;
    return sResult;
}


static gboolean cb_emblem(gpointer data)
{
    std::cout << "timer!!" << std::endl;

    auto updateHandleData = (nautilus_extension::FileInfoUpdateHandlerData *)data;
    //nautilus_file_info_add_string_attribute(updateHandleData->file, "testattr", "testval");
    std::cout << nautilus_file_info_get_name(updateHandleData->file) << std::endl;

    //nautilus_info_provider_update_complete_invoke(updateHandleData->update_complete, updateHandleData->provider,
    //         (NautilusOperationHandle*)updateHandleData, NAUTILUS_OPERATION_COMPLETE);

    nautilus_file_info_add_emblem(updateHandleData->file, "stock_calc-accept");

    g_closure_unref(updateHandleData->update_complete);
    g_object_unref(updateHandleData->file);
    delete updateHandleData;

    return false;
}


NautilusOperationResult nautilus_extension::updateFileInfo(NautilusInfoProvider *provider, NautilusFileInfo *file,
                                                           GClosure *update_complete, NautilusOperationHandle **handle)
{
    CloudMailRuExtension* extension = ((CloudMailRuExtension_Glue *)provider)->extension;
    if (extension == nullptr)  return NAUTILUS_OPERATION_COMPLETE;   // no valid initialized extension class

    /*
    auto updateHandleData = new nautilus_extension::FileInfoUpdateHandlerData();
    updateHandleData->update_complete = g_closure_ref(update_complete);
    updateHandleData->provider = provider;
    updateHandleData->file = (NautilusFileInfo*) g_object_ref(file);
    updateHandleData->operation_handle = rand() % 100000;
    *handle = (NautilusOperationHandle *)updateHandleData;
    */

    auto fileInfo = new nautilus_extension::NautilusFileInfo_Glue(file);
    extension->updateFileInfo(fileInfo);

//    g_timeout_add_seconds(3, cb_emblem, updateHandleData);
    return NAUTILUS_OPERATION_COMPLETE;
}
