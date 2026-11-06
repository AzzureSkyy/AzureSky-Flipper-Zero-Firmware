#include "../morse_beacon_app_i.h"

void morse_beacon_scene_input_on_enter(void* context) {
    MorseBeaconApp* app = context;
    TextInput* text_input = app->text_input;

    strlcpy(app->text_store, app->text, MORSE_BEACON_TEXT_STORE_SIZE + 1);

    text_input_set_header_text(text_input, "Morse message");
    text_input_set_result_callback(
        text_input,
        morse_beacon_text_input_callback,
        app,
        app->text_store,
        MORSE_BEACON_TEXT_MAX_LENGTH + 1,
        false);

    text_input_set_minimum_length(text_input, 1);

    view_dispatcher_switch_to_view(app->view_dispatcher, MorseBeaconViewTextInput);
}

bool morse_beacon_scene_input_on_event(void* context, SceneManagerEvent event) {
    MorseBeaconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MorseBeaconCustomEventTextEditDone) {
            strlcpy(
                app->text,
                app->text_store,
                MORSE_BEACON_TEXT_MAX_LENGTH + 1);
            morse_beacon_save_text(app);
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

void morse_beacon_scene_input_on_exit(void* context) {
    MorseBeaconApp* app = context;
    text_input_reset(app->text_input);
}
