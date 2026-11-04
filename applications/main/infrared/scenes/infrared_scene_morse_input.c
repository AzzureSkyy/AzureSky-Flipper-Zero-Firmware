#include "../infrared_app_i.h"

void infrared_scene_morse_input_on_enter(void* context) {
    InfraredApp* infrared = context;
    TextInput* text_input = infrared->text_input;

    strlcpy(infrared->text_store[0], infrared->morse_text, INFRARED_TEXT_STORE_SIZE + 1);

    text_input_set_header_text(text_input, "Morse message");
    text_input_set_result_callback(
        text_input,
        infrared_text_input_callback,
        infrared,
        infrared->text_store[0],
        INFRARED_MORSE_TEXT_MAX_LENGTH + 1,
        false);

    text_input_set_minimum_length(text_input, 1);

    view_dispatcher_switch_to_view(infrared->view_dispatcher, InfraredViewTextInput);
}

bool infrared_scene_morse_input_on_event(void* context, SceneManagerEvent event) {
    InfraredApp* infrared = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == InfraredCustomEventTypeTextEditDone) {
            strlcpy(
                infrared->morse_text,
                infrared->text_store[0],
                INFRARED_MORSE_TEXT_MAX_LENGTH + 1);
            infrared_morse_save_text(infrared);
            scene_manager_previous_scene(infrared->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

void infrared_scene_morse_input_on_exit(void* context) {
    InfraredApp* infrared = context;
    text_input_reset(infrared->text_input);
}
