#include "morse_beacon_app_i.h"

#include <power/power_service/power.h>
#include <toolbox/saved_struct.h>
#include <dolphin/dolphin.h>

#include <string.h>
#include <ctype.h>

#define TAG "MorseBeaconApp"

#define MORSE_BEACON_SETTINGS_PATH    INT_PATH(".morse_beacon.settings")
#define MORSE_BEACON_SETTINGS_VERSION (1)
#define MORSE_BEACON_SETTINGS_MAGIC   (0x4D)

typedef struct {
    FuriHalInfraredTxPin tx_pin;
    bool otg_enabled;
    uint8_t wpm;
} MorseBeaconSettings;

static bool morse_beacon_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    MorseBeaconApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool morse_beacon_back_event_callback(void* context) {
    furi_assert(context);
    MorseBeaconApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void morse_beacon_tick_event_callback(void* context) {
    furi_assert(context);
    MorseBeaconApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

void morse_beacon_text_store_set(MorseBeaconApp* app, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(app->text_store, MORSE_BEACON_TEXT_STORE_SIZE, fmt, args);
    va_end(args);
}

void morse_beacon_text_input_callback(void* context) {
    furi_assert(context);
    MorseBeaconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, MorseBeaconCustomEventTextEditDone);
}

void morse_beacon_show_error_message(MorseBeaconApp* app, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    FuriString* message = furi_string_alloc_vprintf(fmt, args);
    dialog_message_show_storage_error(app->dialogs, furi_string_get_cstr(message));

    furi_string_free(message);
    va_end(args);
}

void morse_beacon_enable_otg(MorseBeaconApp* app, bool enable) {
    Power* power = furi_record_open(RECORD_POWER);

    power_enable_otg(power, enable);
    app->state.is_otg_enabled = enable;

    furi_record_close(RECORD_POWER);
}

void morse_beacon_set_tx_pin(MorseBeaconApp* app, FuriHalInfraredTxPin tx_pin) {
    if(tx_pin < FuriHalInfraredTxPinMax) {
        furi_hal_infrared_set_tx_output(tx_pin);
    } else {
        FuriHalInfraredTxPin tx_pin_detected = furi_hal_infrared_detect_tx_output();
        furi_hal_infrared_set_tx_output(tx_pin_detected);
        if(tx_pin_detected != FuriHalInfraredTxPinInternal) {
            morse_beacon_enable_otg(app, true);
        }
    }

    app->state.tx_pin = tx_pin;
}

static void morse_beacon_load_settings(MorseBeaconApp* app) {
    MorseBeaconSettings settings = {
        .tx_pin = FuriHalInfraredTxPinInternal,
        .otg_enabled = false,
        .wpm = 15,
    };

    if(!saved_struct_load(
           MORSE_BEACON_SETTINGS_PATH,
           &settings,
           sizeof(MorseBeaconSettings),
           MORSE_BEACON_SETTINGS_MAGIC,
           MORSE_BEACON_SETTINGS_VERSION)) {
        FURI_LOG_D(TAG, "Failed to load settings, using defaults");
    }

    app->state.wpm = settings.wpm ? settings.wpm : 15;
    morse_beacon_set_tx_pin(app, settings.tx_pin);
    if(settings.tx_pin < FuriHalInfraredTxPinMax) {
        morse_beacon_enable_otg(app, settings.otg_enabled);
    }
}

void morse_beacon_save_settings(MorseBeaconApp* app) {
    MorseBeaconSettings settings = {
        .tx_pin = app->state.tx_pin,
        .otg_enabled = app->state.is_otg_enabled,
        .wpm = app->state.wpm,
    };

    if(!saved_struct_save(
           MORSE_BEACON_SETTINGS_PATH,
           &settings,
           sizeof(MorseBeaconSettings),
           MORSE_BEACON_SETTINGS_MAGIC,
           MORSE_BEACON_SETTINGS_VERSION)) {
        FURI_LOG_E(TAG, "Failed to save settings");
    }
}

bool morse_beacon_build_signal(MorseBeaconApp* app, const char* text, uint8_t wpm) {
    if(!app || !text || !text[0]) {
        return false;
    }

    size_t timings_size = 0;
    const bool ok = morse_beacon_encode(
        text, wpm, app->timings, MORSE_BEACON_MAX_TIMINGS, &timings_size);

    if(!ok || !timings_size) {
        return false;
    }

    app->timings_count = timings_size;
    return true;
}

bool morse_beacon_load_text(MorseBeaconApp* app) {
    return morse_beacon_load_text_from(app, MORSE_BEACON_TEXT_PATH);
}

bool morse_beacon_load_text_from(MorseBeaconApp* app, const char* path) {
    furi_assert(app);
    furi_assert(path);

    File* file = storage_file_alloc(app->storage);
    bool loaded = false;
    char buffer[MORSE_BEACON_TEXT_MAX_LENGTH + 1] = {0};

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        const size_t read_size = storage_file_read(file, buffer, MORSE_BEACON_TEXT_MAX_LENGTH);
        buffer[read_size] = '\0';

        for(size_t i = 0; i < read_size; i++) {
            if((buffer[i] == '\r') || (buffer[i] == '\n')) {
                buffer[i] = '\0';
                break;
            }
        }

        if(buffer[0] != '\0') {
            strlcpy(app->text, buffer, sizeof(app->text));
            loaded = true;
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    return loaded;
}

bool morse_beacon_save_text(MorseBeaconApp* app) {
    furi_assert(app);

    storage_simply_mkdir(app->storage, MORSE_BEACON_TEXT_DIR);

    File* file = storage_file_alloc(app->storage);
    bool saved = false;

    if(storage_file_open(file, MORSE_BEACON_TEXT_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* text = app->text;
        const size_t text_len = strlen(text);
        const size_t written = storage_file_write(file, text, text_len);
        saved = (written == text_len);
    }

    storage_file_close(file);
    storage_file_free(file);

    return saved;
}

bool morse_beacon_save_text_named(MorseBeaconApp* app, FuriString* saved_path) {
    furi_assert(app);
    furi_assert(saved_path);

    /* Build a file name from the message: letters/digits kept, spaces -> '_' */
    char name[MORSE_BEACON_SAVE_NAME_MAX + 1] = {0};
    size_t len = 0;
    for(const char* c = app->text; *c && len < MORSE_BEACON_SAVE_NAME_MAX; c++) {
        if(isalnum((unsigned char)*c) || *c == '-' || *c == '_') {
            name[len++] = *c;
        } else if(*c == ' ' && len > 0 && name[len - 1] != '_') {
            name[len++] = '_';
        }
    }
    while(len > 0 && name[len - 1] == '_') {
        name[--len] = '\0';
    }
    if(len == 0) {
        strlcpy(name, "message", sizeof(name));
    }

    storage_simply_mkdir(app->storage, MORSE_BEACON_TEXT_DIR);
    storage_simply_mkdir(app->storage, MORSE_BEACON_SAVES_DIR);

    furi_string_printf(saved_path, MORSE_BEACON_SAVES_DIR "/%s.txt", name);

    File* file = storage_file_alloc(app->storage);
    bool saved = false;

    if(storage_file_open(
           file, furi_string_get_cstr(saved_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* text = app->text;
        const size_t text_len = strlen(text);
        const size_t written = storage_file_write(file, text, text_len);
        saved = (written == text_len);
    }

    storage_file_close(file);
    storage_file_free(file);

    return saved;
}

/* ISR data callback: loops the Morse timings forever (mark, space, ...,
   mark, inter-repeat gap) and ends at the very next timing once stop is
   requested, so Stop never waits for the rest of the message. */
static FuriHalInfraredTxGetDataState
    morse_beacon_tx_data_callback(void* context, uint32_t* duration, bool* level) {
    MorseBeaconApp* app = context;

    if(app->stop_requested) {
        *level = false;
        *duration = 1000;
        return FuriHalInfraredTxGetDataStateLastDone;
    }

    const size_t count = app->timings_count;
    const size_t i = app->tx_index;

    if(i < count) {
        *level = !(i % 2);
        *duration = app->timings[i];
        app->tx_index = i + 1;
        return FuriHalInfraredTxGetDataStateDone;
    } else {
        /* Word gap (7 units) between repeats; timings[0] is one dot unit */
        *level = false;
        *duration = 7 * (1200000U / MAX(app->state.wpm, 5));
        app->tx_index = 0;
        return FuriHalInfraredTxGetDataStateDone;
    }
}

void morse_beacon_start(MorseBeaconApp* app) {
    if(!app || !app->state.is_running) {
        return;
    }

    if(!morse_beacon_build_signal(app, app->text, app->state.wpm) ||
       (app->timings_count % 2) == 0) {
        app->state.is_running = false;
        return;
    }

    if(furi_hal_infrared_is_busy()) {
        /* Already transmitting this beacon; keep running state and UI as Stop. */
        return;
    }

    app->state.last_start_tick = furi_get_tick();
    morse_beacon_set_tx_pin(app, app->state.tx_pin);

    app->stop_requested = false;
    app->tx_index = 0;
    furi_hal_infrared_async_tx_set_data_isr_callback(morse_beacon_tx_data_callback, app);
    furi_hal_infrared_async_tx_start(MORSE_BEACON_CARRIER_FREQUENCY, MORSE_BEACON_DUTY_CYCLE);

    dolphin_deed(DolphinDeedIrSend);
    notification_message(app->notifications, &sequence_blink_start_magenta);
}

void morse_beacon_stop(MorseBeaconApp* app) {
    if(!app) {
        return;
    }

    app->state.is_running = false;
    app->stop_requested = true;

    if(furi_hal_infrared_is_busy()) {
        furi_hal_infrared_async_tx_stop();
    }

    furi_hal_infrared_async_tx_set_data_isr_callback(NULL, NULL);
    notification_message(app->notifications, &sequence_blink_stop);
}

static MorseBeaconApp* morse_beacon_alloc(void) {
    MorseBeaconApp* app = malloc(sizeof(MorseBeaconApp));

    app->state.is_running = false;
    app->state.is_otg_enabled = false;
    app->state.last_start_tick = 0;
    app->state.tx_pin = FuriHalInfraredTxPinInternal;
    app->state.wpm = 15;

    strlcpy(app->text, "SOS", sizeof(app->text));
    app->timings_count = 0;
    app->stop_requested = false;
    app->tx_index = 0;

    app->scene_manager = scene_manager_alloc(&morse_beacon_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();

    app->gui = furi_record_open(RECORD_GUI);

    ViewDispatcher* view_dispatcher = app->view_dispatcher;
    view_dispatcher_set_event_callback_context(view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(view_dispatcher, morse_beacon_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(view_dispatcher, morse_beacon_back_event_callback);
    view_dispatcher_set_tick_event_callback(view_dispatcher, morse_beacon_tick_event_callback, 100);

    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        view_dispatcher, MorseBeaconViewTextInput, text_input_get_view(app->text_input));

    app->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        view_dispatcher, MorseBeaconViewDialogEx, dialog_ex_get_view(app->dialog_ex));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        view_dispatcher,
        MorseBeaconViewVariableList,
        variable_item_list_get_view(app->var_item_list));

    return app;
}

static void morse_beacon_free(MorseBeaconApp* app) {
    furi_assert(app);
    ViewDispatcher* view_dispatcher = app->view_dispatcher;

    view_dispatcher_remove_view(view_dispatcher, MorseBeaconViewTextInput);
    text_input_free(app->text_input);

    view_dispatcher_remove_view(view_dispatcher, MorseBeaconViewDialogEx);
    dialog_ex_free(app->dialog_ex);

    view_dispatcher_remove_view(view_dispatcher, MorseBeaconViewVariableList);
    variable_item_list_free(app->var_item_list);

    view_dispatcher_free(view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;

    furi_record_close(RECORD_DIALOGS);
    app->dialogs = NULL;

    furi_record_close(RECORD_GUI);
    app->gui = NULL;

    furi_record_close(RECORD_STORAGE);
    app->storage = NULL;

    free(app);
}

int32_t morse_beacon_app(void* p) {
    UNUSED(p);

    MorseBeaconApp* app = morse_beacon_alloc();

    morse_beacon_load_settings(app);
    morse_beacon_load_text(app);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, MorseBeaconSceneConfig);

    view_dispatcher_run(app->view_dispatcher);

    morse_beacon_stop(app);
    furi_hal_infrared_set_tx_output(FuriHalInfraredTxPinInternal);
    if(app->state.is_otg_enabled) {
        morse_beacon_enable_otg(app, false);
    }

    morse_beacon_free(app);

    return 0;
}
