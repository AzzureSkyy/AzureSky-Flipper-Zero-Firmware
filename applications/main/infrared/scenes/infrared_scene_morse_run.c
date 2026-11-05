#include "../infrared_app_i.h"

static const char* infrared_scene_morse_tx_name(FuriHalInfraredTxPin tx_pin) {
    if(tx_pin == FuriHalInfraredTxPinInternal) {
        return "Flipper";
    } else if(tx_pin == FuriHalInfraredTxPinExtPA7) {
        return "2(A7)";
    }

    return "Detect";
}

static void infrared_scene_morse_run_dialog_callback(DialogExResult result, void* context) {
    InfraredApp* infrared = context;
    view_dispatcher_send_custom_event(infrared->view_dispatcher, result);
}

static void infrared_scene_morse_run_update_dialog(InfraredApp* infrared) {
    DialogEx* dialog_ex = infrared->dialog_ex;

    const bool is_running = infrared->app_state.is_morse_beacon_running;
    const char* tx_name = infrared_scene_morse_tx_name(infrared->app_state.tx_pin);

    infrared_text_store_set(
        infrared,
        0,
        "Msg: %s\nWPM: %u\nTX: %s\nState: %s",
        infrared->morse_text,
        infrared->app_state.morse_wpm,
        tx_name,
        is_running ? "Running" : "Stopped");

    dialog_ex_set_header(dialog_ex, "IR Morse Beacon", 64, 1, AlignCenter, AlignTop);
    dialog_ex_set_text(dialog_ex, infrared->text_store[0], 5, 13, AlignLeft, AlignTop);
    dialog_ex_set_left_button_text(dialog_ex, "Edit");
    dialog_ex_set_center_button_text(dialog_ex, is_running ? "Stop" : "Start");
    dialog_ex_set_right_button_text(dialog_ex, "TX");
}

void infrared_scene_morse_run_on_enter(void* context) {
    InfraredApp* infrared = context;
    DialogEx* dialog_ex = infrared->dialog_ex;

    infrared_scene_morse_run_update_dialog(infrared);
    dialog_ex_set_result_callback(dialog_ex, infrared_scene_morse_run_dialog_callback);
    dialog_ex_set_context(dialog_ex, infrared);
    dialog_ex_enable_extended_events(dialog_ex);

    view_dispatcher_switch_to_view(infrared->view_dispatcher, InfraredViewDialogEx);
}

bool infrared_scene_morse_run_on_event(void* context, SceneManagerEvent event) {
    InfraredApp* infrared = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        if(infrared->app_state.is_morse_beacon_running) {
            infrared_morse_start(infrared);
            infrared_scene_morse_run_update_dialog(infrared);
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        infrared_morse_stop(infrared);
        scene_manager_previous_scene(infrared->scene_manager);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DialogExResultLeft) {
            infrared_morse_stop(infrared);
            scene_manager_next_scene(infrared->scene_manager, InfraredSceneMorseInput);
            consumed = true;
        } else if(event.event == DialogExResultCenter) {
            if(infrared->app_state.is_morse_beacon_running) {
                infrared_morse_stop(infrared);
            } else {
                infrared->app_state.is_morse_beacon_running = true;
                infrared->app_state.morse_last_start_tick = 0;
                infrared_morse_start(infrared);
            }
            infrared_scene_morse_run_update_dialog(infrared);
            consumed = true;
        } else if(event.event == DialogExResultRight) {
            uint8_t tx_pin = infrared->app_state.tx_pin;
            tx_pin = (tx_pin + 1) % (FuriHalInfraredTxPinMax + 1);
            infrared_set_tx_pin(infrared, tx_pin);
            infrared_save_settings(infrared);
            if(infrared->app_state.is_morse_beacon_running) {
                infrared_morse_stop(infrared);
                infrared->app_state.is_morse_beacon_running = true;
                infrared_morse_start(infrared);
            }
            infrared_scene_morse_run_update_dialog(infrared);
            consumed = true;
        }
    }

    return consumed;
}

void infrared_scene_morse_run_on_exit(void* context) {
    InfraredApp* infrared = context;
    infrared_morse_stop(infrared);
    dialog_ex_reset(infrared->dialog_ex);
}
