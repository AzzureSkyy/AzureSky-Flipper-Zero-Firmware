#include "../infrared_app_i.h"

enum InfraredMorseConfigIndex {
    InfraredMorseConfigIndexStart,
    InfraredMorseConfigIndexText,
    InfraredMorseConfigIndexWpm,
    InfraredMorseConfigIndexTxOutput,
};

static const char* const infrared_morse_wpm_text[] = {"8", "10", "12", "15", "18", "20", "25"};
static const uint8_t infrared_morse_wpm_values[] = {8, 10, 12, 15, 18, 20, 25};

static const char* const infrared_morse_tx_text[] = {
    "Flipper",
    "2 (A7)",
    "Detect",
};

static void infrared_scene_morse_config_enter_callback(void* context, uint32_t index) {
    InfraredApp* infrared = context;
    view_dispatcher_send_custom_event(infrared->view_dispatcher, index);
}

static void infrared_scene_morse_config_wpm_callback(VariableItem* item) {
    InfraredApp* infrared = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    infrared->app_state.morse_wpm = infrared_morse_wpm_values[index];
    variable_item_set_current_value_text(item, infrared_morse_wpm_text[index]);
}

static void infrared_scene_morse_config_tx_callback(VariableItem* item) {
    InfraredApp* infrared = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    infrared_set_tx_pin(infrared, index);
    variable_item_set_current_value_text(item, infrared_morse_tx_text[index]);
}

static uint8_t infrared_scene_morse_config_find_wpm_index(uint8_t wpm) {
    for(uint8_t i = 0; i < COUNT_OF(infrared_morse_wpm_values); i++) {
        if(infrared_morse_wpm_values[i] == wpm) {
            return i;
        }
    }
    return 0;
}

static void infrared_scene_morse_config_setup(InfraredApp* infrared) {
    VariableItemList* var_item_list = infrared->var_item_list;
    VariableItem* item;

    variable_item_list_add(var_item_list, "Start Beacon", 0, NULL, NULL);
    variable_item_list_add(var_item_list, "Message Text", 0, NULL, NULL);

    item = variable_item_list_add(
        var_item_list,
        "WPM",
        COUNT_OF(infrared_morse_wpm_values),
        infrared_scene_morse_config_wpm_callback,
        infrared);

    const uint8_t wpm_index =
        infrared_scene_morse_config_find_wpm_index(infrared->app_state.morse_wpm);
    variable_item_set_current_value_index(item, wpm_index);
    variable_item_set_current_value_text(item, infrared_morse_wpm_text[wpm_index]);

    item = variable_item_list_add(
        var_item_list,
        "Signal Output",
        COUNT_OF(infrared_morse_tx_text),
        infrared_scene_morse_config_tx_callback,
        infrared);

    uint8_t tx_index = infrared->app_state.tx_pin;
    if(tx_index >= COUNT_OF(infrared_morse_tx_text)) {
        tx_index = FuriHalInfraredTxPinMax;
    }

    variable_item_set_current_value_index(item, tx_index);
    variable_item_set_current_value_text(item, infrared_morse_tx_text[tx_index]);
}

void infrared_scene_morse_config_on_enter(void* context) {
    InfraredApp* infrared = context;
    VariableItemList* var_item_list = infrared->var_item_list;

    variable_item_list_set_enter_callback(
        var_item_list, infrared_scene_morse_config_enter_callback, infrared);
    infrared_scene_morse_config_setup(infrared);

    const uint32_t selected_item =
        scene_manager_get_scene_state(infrared->scene_manager, InfraredSceneMorseConfig);
    variable_item_list_set_selected_item(var_item_list, selected_item);

    view_dispatcher_switch_to_view(infrared->view_dispatcher, InfraredViewVariableList);
}

bool infrared_scene_morse_config_on_event(void* context, SceneManagerEvent event) {
    InfraredApp* infrared = context;
    SceneManager* scene_manager = infrared->scene_manager;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(scene_manager, InfraredSceneMorseConfig, event.event);
        consumed = true;

        if(event.event == InfraredMorseConfigIndexStart) {
            scene_manager_next_scene(scene_manager, InfraredSceneMorseRun);
        } else if(event.event == InfraredMorseConfigIndexText) {
            scene_manager_next_scene(scene_manager, InfraredSceneMorseInput);
        }
    }

    return consumed;
}

void infrared_scene_morse_config_on_exit(void* context) {
    InfraredApp* infrared = context;
    variable_item_list_reset(infrared->var_item_list);
    infrared_save_settings(infrared);
}
