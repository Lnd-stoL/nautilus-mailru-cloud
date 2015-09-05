
#include <iostream>
#include <string>
using std::string;

#include <libnautilus-extension/nautilus-column-provider.h>
#include <libnautilus-extension/nautilus-info-provider.h>
#include <libnautilus-extension/nautilus-menu-provider.h>

#include "extension_info.hpp"
#include "CloudMailRuExtension.hpp"
#include "URLTools.hpp"

#include <boost/property_tree/ptree.hpp>
namespace b_pt = boost::property_tree;

//----------------------------------------------------------------------------------------------------------------------

namespace nautilus_extension
{
    static GType extensionExportedType;

    // access to global extension class instance
    static CloudMailRuExtension *singleExtensionInstance = nullptr;


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
                _path = URLTools::decode(uri());
                auto pathSubstr = _path.substr(_path.find(':') + 3);
                _path.swap(pathSubstr);
            }
            return _path;
        }


        inline virtual bool setSyncState(sync_state_t state) {
            auto prevSyncState = _syncState;

            if (FileInfo::setSyncState(state)) {

                if (prevSyncState != UNKNOWN) {    // this is to fix the emblems doubleing
                    invalidateExtensionInfo();
                    return true;
                }

                if (state == FileInfo::sync_state_t::ACTUAL) {
                    nautilus_file_info_add_emblem(_fileInfo,
                                                  singleExtensionInstance->config().get<string>("Emblems.sync_completed").c_str());
                }
                if (state == FileInfo::sync_state_t::IN_PROGRESS) {
                    nautilus_file_info_add_emblem(_fileInfo,
                                                  singleExtensionInstance->config().get<string>("Emblems.sync_in_progress").c_str());
                }
                if (state == FileInfo::sync_state_t::ACTUAL_SHARED) {
                    nautilus_file_info_add_emblem(_fileInfo,
                                                  singleExtensionInstance->config().get<string>("Emblems.sync_shared").c_str());
                }

                return true;
            }

            return false;
        }


        inline virtual bool isStillActual() {
            return ! (bool) nautilus_file_info_is_gone(_fileInfo);
        }


        inline virtual void invalidateExtensionInfo() {
            nautilus_file_info_invalidate_extension_info(_fileInfo);
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
        std::cout << "! " << extensionErr.what() << std::endl;
        extensionInstance->extension = nullptr;
    }
    catch(std::exception err) {
        std::cout << "!" << err.what() << std::endl;
        extensionInstance->extension = nullptr;
    }

    nautilus_extension::singleExtensionInstance = extensionInstance->extension;
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
    auto fileInfo = new NautilusFileInfo_Glue(NAUTILUS_FILE_INFO(files->data));
    extension->getContextMenuItemsForFile(fileInfo, items);
    if (items.empty())  return nullptr;   // no menu items to add

    NautilusMenuItem *mailruSubmenuItem = nautilus_menu_item_new("CloudMailRuExtension::MailRuSubmenu", "Облако Mail.Ru",
                                                    "Cloud@Mail.Ru related actions",
                                                    nullptr);
    NautilusMenu *mailruSubmenu = nautilus_menu_new();
    nautilus_menu_item_set_submenu(mailruSubmenuItem, mailruSubmenu);
    GList *itemsList = g_list_append(nullptr, mailruSubmenuItem);

    for (auto const & absItem : items) {
        static const string itemIdStrPrefix = "CloudMailRuExtension::";
        NautilusMenuItem *item = nautilus_menu_item_new((itemIdStrPrefix + absItem.name()).c_str(), absItem.title().c_str(),
                                      absItem.description().c_str(), (absItem.icon() == "" ? nullptr : absItem.icon().c_str()));

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
    (*handler)();
    delete handler;
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
