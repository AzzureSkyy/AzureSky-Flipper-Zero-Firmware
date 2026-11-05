#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>
#include <storage/storage.h>
#include <power/power_service/power.h>
#include <m-array.h>

#define THEMES_DIR      EXT_PATH("themes")
#define THEMES_CURRENT  THEMES_DIR "/.current"
#define THEMES_DEFAULT  "Dolphin"
#define THEMES_NAME_MAX 64

ARRAY_DEF(ThemeList, FuriString*, FURI_STRING_OPLIST)

typedef enum {
    ThemesViewSubmenu,
    ThemesViewConfirm,
} ThemesView;

typedef struct {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    DialogEx* dialog;
    ThemeList_t themes;
    FuriString* current;
    uint32_t selected;
    FuriString* header;
} ThemesApp;

static void themes_read_current(ThemesApp* app) {
    furi_string_set(app->current, THEMES_DEFAULT);
    char name[THEMES_NAME_MAX] = {0};
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, THEMES_CURRENT, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t read = storage_file_read(file, name, sizeof(name) - 1);
        name[read] = '\0';
    }
    storage_file_close(file);
    storage_file_free(file);
    for(size_t i = 0; name[i]; i++) {
        if(name[i] == '\r' || name[i] == '\n') {
            name[i] = '\0';
            break;
        }
    }
    if(name[0]) furi_string_set(app->current, name);
}

static bool themes_write_current(ThemesApp* app, const char* name) {
    storage_simply_mkdir(app->storage, THEMES_DIR);
    File* file = storage_file_alloc(app->storage);
    bool ok = false;
    if(storage_file_open(file, THEMES_CURRENT, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t len = strlen(name);
        ok = storage_file_write(file, name, len) == len;
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static void themes_add(ThemesApp* app, const char* name) {
    FuriString* tmp = furi_string_alloc_set(name);
    ThemeList_push_back(app->themes, tmp);
    furi_string_free(tmp);
}

static void themes_scan(ThemesApp* app) {
    themes_add(app, THEMES_DEFAULT);

    storage_simply_mkdir(app->storage, THEMES_DIR);
    File* dir = storage_file_alloc(app->storage);
    FileInfo info;
    char name[THEMES_NAME_MAX];
    FuriString* manifest = furi_string_alloc();

    if(storage_dir_open(dir, THEMES_DIR)) {
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(!file_info_is_dir(&info) || name[0] == '.') continue;
            if(!strcmp(name, THEMES_DEFAULT)) continue;
            furi_string_printf(manifest, THEMES_DIR "/%s/manifest.txt", name);
            if(storage_common_stat(app->storage, furi_string_get_cstr(manifest), NULL) != FSE_OK)
                continue;
            themes_add(app, name);
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    furi_string_free(manifest);
}

static void themes_confirm_callback(DialogExResult result, void* context) {
    ThemesApp* app = context;
    if(result == DialogExResultRight) {
        const char* name = furi_string_get_cstr(*ThemeList_get(app->themes, app->selected));
        if(themes_write_current(app, name)) {
            Power* power = furi_record_open(RECORD_POWER);
            power_reboot(power, PowerBootModeNormal);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, ThemesViewSubmenu);
}

static void themes_submenu_callback(void* context, uint32_t index) {
    ThemesApp* app = context;
    app->selected = index;
    furi_string_printf(
        app->header, "Apply %s?", furi_string_get_cstr(*ThemeList_get(app->themes, index)));
    dialog_ex_set_header(
        app->dialog, furi_string_get_cstr(app->header), 64, 10, AlignCenter, AlignCenter);
    dialog_ex_set_text(
        app->dialog, "Flipper will reboot\nto load animations", 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog, "Cancel");
    dialog_ex_set_right_button_text(app->dialog, "Apply");
    dialog_ex_set_result_callback(app->dialog, themes_confirm_callback);
    dialog_ex_set_context(app->dialog, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ThemesViewConfirm);
}

static bool themes_back_callback(void* context) {
    ThemesApp* app = context;
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static uint32_t themes_confirm_previous(void* context) {
    UNUSED(context);
    return ThemesViewSubmenu;
}

int32_t themes_app(void* p) {
    UNUSED(p);
    ThemesApp* app = malloc(sizeof(ThemesApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->current = furi_string_alloc();
    app->header = furi_string_alloc();
    ThemeList_init(app->themes);

    themes_read_current(app);
    themes_scan(app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, themes_back_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "Choose Theme");
    uint32_t current_index = 0;
    FuriString* label = furi_string_alloc();
    for(size_t i = 0; i < ThemeList_size(app->themes); i++) {
        const char* name = furi_string_get_cstr(*ThemeList_get(app->themes, i));
        bool active = !strcmp(name, furi_string_get_cstr(app->current));
        if(active) current_index = i;
        furi_string_printf(label, "%s%s", active ? "* " : "", name);
        submenu_add_item(
            app->submenu, furi_string_get_cstr(label), i, themes_submenu_callback, app);
    }
    furi_string_free(label);
    submenu_set_selected_item(app->submenu, current_index);
    view_dispatcher_add_view(
        app->view_dispatcher, ThemesViewSubmenu, submenu_get_view(app->submenu));

    app->dialog = dialog_ex_alloc();
    view_set_previous_callback(dialog_ex_get_view(app->dialog), themes_confirm_previous);
    view_dispatcher_add_view(
        app->view_dispatcher, ThemesViewConfirm, dialog_ex_get_view(app->dialog));

    view_dispatcher_switch_to_view(app->view_dispatcher, ThemesViewSubmenu);
    view_dispatcher_run(app->view_dispatcher);

    view_dispatcher_remove_view(app->view_dispatcher, ThemesViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, ThemesViewConfirm);
    submenu_free(app->submenu);
    dialog_ex_free(app->dialog);
    view_dispatcher_free(app->view_dispatcher);

    ThemeList_clear(app->themes);
    furi_string_free(app->current);
    furi_string_free(app->header);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
