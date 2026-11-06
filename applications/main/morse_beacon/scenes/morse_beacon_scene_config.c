#include "../morse_beacon_app_i.h"

enum MorseBeaconConfigIndex {
    MorseBeaconConfigIndexStart,
    MorseBeaconConfigIndexText,
    MorseBeaconConfigIndexSaveText,
    MorseBeaconConfigIndexLoadText,
    MorseBeaconConfigIndexWpm,
    MorseBeaconConfigIndexTxOutput,
};

static const char* const morse_beacon_wpm_text[] = {"8", "10", "12", "15", "18", "20", "25"};
static const uint8_t morse_beacon_wpm_values[] = {8, 10, 12, 15, 18, 20, 25};

static const char* const morse_beacon_tx_text[] = {
    "Flipper",
    "2 (A7)",
    "Detect",
};

static void morse_beacon_scene_config_enter_callback(void* context, uint32_t index) {
    MorseBeaconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void morse_beacon_scene_config_wpm_callback(VariableItem* item) {
    MorseBeaconApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->state.wpm = morse_beacon_wpm_values[index];
    variable_item_set_current_value_text(item, morse_beacon_wpm_text[index]);
}

static void morse_beacon_scene_config_tx_callback(VariableItem* item) {
    MorseBeaconApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    morse_beacon_set_tx_pin(app, index);
    variable_item_set_current_value_text(item, morse_beacon_tx_text[index]);
}

static uint8_t morse_beacon_scene_config_find_wpm_index(uint8_t wpm) {
    for(uint8_t i = 0; i < COUNT_OF(morse_beacon_wpm_values); i++) {
        if(morse_beacon_wpm_values[i] == wpm) {
            return i;
        }
    }
    return 0;
}

static void morse_beacon_scene_config_setup(MorseBeaconApp* app) {
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    variable_item_list_add(var_item_list, "Start Beacon", 0, NULL, NULL);
    variable_item_list_add(var_item_list, "Message Text", 0, NULL, NULL);
    variable_item_list_add(var_item_list, "Save Text to File", 0, NULL, NULL);
    variable_item_list_add(var_item_list, "Load Text from File", 0, NULL, NULL);

    item = variable_item_list_add(
        var_item_list,
        "WPM",
        COUNT_OF(morse_beacon_wpm_values),
        morse_beacon_scene_config_wpm_callback,
        app);

    const uint8_t wpm_index =
        morse_beacon_scene_config_find_wpm_index(app->state.wpm);
    variable_item_set_current_value_index(item, wpm_index);
    variable_item_set_current_value_text(item, morse_beacon_wpm_text[wpm_index]);

    item = variable_item_list_add(
        var_item_list,
        "Signal Output",
        COUNT_OF(morse_beacon_tx_text),
        morse_beacon_scene_config_tx_callback,
        app);

    uint8_t tx_index = app->state.tx_pin;
    if(tx_index >= COUNT_OF(morse_beacon_tx_text)) {
        tx_index = FuriHalInfraredTxPinMax;
    }

    variable_item_set_current_value_index(item, tx_index);
    variable_item_set_current_value_text(item, morse_beacon_tx_text[tx_index]);
}

static void morse_beacon_scene_config_show_message(
    MorseBeaconApp* app,
    const char* header,
    const char* text) {
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, header, 64, 2, AlignCenter, AlignTop);
    dialog_message_set_text(message, text, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, NULL, "OK", NULL);
    dialog_message_show(app->dialogs, message);
    dialog_message_free(message);
}

static void morse_beacon_scene_config_save(MorseBeaconApp* app) {
    FuriString* path = furi_string_alloc();
    if(morse_beacon_save_text_named(app, path)) {
        const char* file_name = strrchr(furi_string_get_cstr(path), '/');
        morse_beacon_scene_config_show_message(
            app, "Text Saved", file_name ? file_name + 1 : furi_string_get_cstr(path));
    } else {
        morse_beacon_show_error_message(app, "Failed to save\nMorse text file");
    }
    furi_string_free(path);
}

static void morse_beacon_scene_config_load(MorseBeaconApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, MORSE_BEACON_TEXT_DIR);
    storage_simply_mkdir(storage, MORSE_BEACON_SAVES_DIR);
    furi_record_close(RECORD_STORAGE);

    FuriString* path = furi_string_alloc_set(MORSE_BEACON_SAVES_DIR);
    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".txt", NULL);
    browser_options.base_path = MORSE_BEACON_SAVES_DIR;

    if(dialog_file_browser_show(app->dialogs, path, path, &browser_options)) {
        if(morse_beacon_load_text_from(app, furi_string_get_cstr(path))) {
            morse_beacon_scene_config_show_message(
                app, "Text Loaded", app->text);
        } else {
            morse_beacon_show_error_message(app, "File is empty or\ncannot be read");
        }
    }

    furi_string_free(path);
}

void morse_beacon_scene_config_on_enter(void* context) {
    MorseBeaconApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;

    variable_item_list_set_enter_callback(
        var_item_list, morse_beacon_scene_config_enter_callback, app);
    morse_beacon_scene_config_setup(app);

    const uint32_t selected_item =
        scene_manager_get_scene_state(app->scene_manager, MorseBeaconSceneConfig);
    variable_item_list_set_selected_item(var_item_list, selected_item);

    view_dispatcher_switch_to_view(app->view_dispatcher, MorseBeaconViewVariableList);
}

bool morse_beacon_scene_config_on_event(void* context, SceneManagerEvent event) {
    MorseBeaconApp* app = context;
    SceneManager* scene_manager = app->scene_manager;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(scene_manager, MorseBeaconSceneConfig, event.event);
        consumed = true;

        if(event.event == MorseBeaconConfigIndexStart) {
            scene_manager_next_scene(scene_manager, MorseBeaconSceneRun);
        } else if(event.event == MorseBeaconConfigIndexText) {
            scene_manager_next_scene(scene_manager, MorseBeaconSceneInput);
        } else if(event.event == MorseBeaconConfigIndexSaveText) {
            morse_beacon_scene_config_save(app);
        } else if(event.event == MorseBeaconConfigIndexLoadText) {
            morse_beacon_scene_config_load(app);
        }
    }

    return consumed;
}

void morse_beacon_scene_config_on_exit(void* context) {
    MorseBeaconApp* app = context;
    variable_item_list_reset(app->var_item_list);
    morse_beacon_save_settings(app);
}
