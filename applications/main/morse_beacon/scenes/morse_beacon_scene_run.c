#include "../morse_beacon_app_i.h"

static const char* morse_beacon_scene_tx_name(FuriHalInfraredTxPin tx_pin) {
    if(tx_pin == FuriHalInfraredTxPinInternal) {
        return "Flipper";
    } else if(tx_pin == FuriHalInfraredTxPinExtPA7) {
        return "2(A7)";
    }

    return "Detect";
}

static void morse_beacon_scene_run_dialog_callback(DialogExResult result, void* context) {
    MorseBeaconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

static void morse_beacon_scene_run_update_dialog(MorseBeaconApp* app) {
    DialogEx* dialog_ex = app->dialog_ex;

    const bool is_running = app->state.is_running;
    const char* tx_name = morse_beacon_scene_tx_name(app->state.tx_pin);

    morse_beacon_text_store_set(app,
        "Msg: %s\nWPM: %u\nTX: %s\nState: %s",
        app->text,
        app->state.wpm,
        tx_name,
        is_running ? "Running" : "Stopped");

    dialog_ex_set_header(dialog_ex, "IR Morse Beacon", 64, 1, AlignCenter, AlignTop);
    dialog_ex_set_text(dialog_ex, app->text_store, 5, 13, AlignLeft, AlignTop);
    dialog_ex_set_left_button_text(dialog_ex, "Edit");
    dialog_ex_set_center_button_text(dialog_ex, is_running ? "Stop" : "Start");
    dialog_ex_set_right_button_text(dialog_ex, "TX");
}

void morse_beacon_scene_run_on_enter(void* context) {
    MorseBeaconApp* app = context;
    DialogEx* dialog_ex = app->dialog_ex;

    morse_beacon_scene_run_update_dialog(app);
    dialog_ex_set_result_callback(dialog_ex, morse_beacon_scene_run_dialog_callback);
    dialog_ex_set_context(dialog_ex, app);
    dialog_ex_disable_extended_events(dialog_ex);

    view_dispatcher_switch_to_view(app->view_dispatcher, MorseBeaconViewDialogEx);
}

bool morse_beacon_scene_run_on_event(void* context, SceneManagerEvent event) {
    MorseBeaconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        if(app->state.is_running) {
            morse_beacon_start(app);
            morse_beacon_scene_run_update_dialog(app);
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        morse_beacon_stop(app);
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DialogExResultLeft) {
            morse_beacon_stop(app);
            scene_manager_next_scene(app->scene_manager, MorseBeaconSceneInput);
            consumed = true;
        } else if(event.event == DialogExResultCenter) {
            if(app->state.is_running) {
                morse_beacon_stop(app);
            } else {
                app->state.is_running = true;
                app->state.last_start_tick = 0;
                morse_beacon_start(app);
            }
            morse_beacon_scene_run_update_dialog(app);
            consumed = true;
        } else if(event.event == DialogExResultRight) {
            uint8_t tx_pin = app->state.tx_pin;
            tx_pin = (tx_pin + 1) % (FuriHalInfraredTxPinMax + 1);
            morse_beacon_set_tx_pin(app, tx_pin);
            morse_beacon_save_settings(app);
            if(app->state.is_running) {
                morse_beacon_stop(app);
                app->state.is_running = true;
                morse_beacon_start(app);
            }
            morse_beacon_scene_run_update_dialog(app);
            consumed = true;
        }
    }

    return consumed;
}

void morse_beacon_scene_run_on_exit(void* context) {
    MorseBeaconApp* app = context;
    morse_beacon_stop(app);
    dialog_ex_reset(app->dialog_ex);
}
