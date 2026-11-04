#include <furi.h>
#include <furi_hal_infrared.h>

#include <gui/gui.h>
#include <input/input.h>

typedef struct {
    char ch;
    const char* code;
} MorseEntry;

typedef enum {
    InfraredMorseTxRouteAuto = 0,
    InfraredMorseTxRouteInternal,
    InfraredMorseTxRouteExtPA7,
    InfraredMorseTxRouteCount,
} InfraredMorseTxRoute;

typedef struct {
    FuriMessageQueue* event_queue;
    ViewPort* view_port;

    bool beacon_enabled;
    uint32_t last_tx_tick;

    uint32_t durations[1024];
    bool levels[1024];
    size_t timings_count;
    size_t tx_index;

    size_t preset_index;
    InfraredMorseTxRoute tx_route;
    FuriHalInfraredTxPin active_tx_pin;
} InfraredMorseBeaconApp;

static const uint32_t kMorseUnitUs = 120000;
static const uint32_t kRepeatDelayMs = 250;
static const uint32_t kIrFreqHz = 38000;
static const float kIrDutyCycle = 0.33f;

static const char* kPresets[] = {
    "SOS",
    "AZURE SKY",
    "FLIPPER ZERO",
    "HELLO WORLD",
};

static const MorseEntry kMorseTable[] = {
    {'A', ".-"},   {'B', "-..."}, {'C', "-.-."}, {'D', "-.."},  {'E', "."},
    {'F', "..-."}, {'G', "--."},  {'H', "...."}, {'I', ".."},   {'J', ".---"},
    {'K', "-.-"},  {'L', ".-.."}, {'M', "--"},   {'N', "-."},   {'O', "---"},
    {'P', ".--."}, {'Q', "--.-"}, {'R', ".-."},  {'S', "..."},  {'T', "-"},
    {'U', "..-"},  {'V', "...-"}, {'W', ".--"},  {'X', "-..-"}, {'Y', "-.--"},
    {'Z', "--.."}, {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
    {'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."}, {'8', "---.."},
    {'9', "----."},
};

static const char* infrared_morse_beacon_route_get_name(InfraredMorseTxRoute route) {
    switch(route) {
    case InfraredMorseTxRouteInternal:
        return "Internal";
    case InfraredMorseTxRouteExtPA7:
        return "Ext PA7";
    case InfraredMorseTxRouteAuto:
    default:
        return "Auto";
    }
}

static void infrared_morse_beacon_apply_tx_route(InfraredMorseBeaconApp* app) {
    if(app->tx_route == InfraredMorseTxRouteAuto) {
        app->active_tx_pin = furi_hal_infrared_detect_tx_output();
    } else if(app->tx_route == InfraredMorseTxRouteExtPA7) {
        app->active_tx_pin = FuriHalInfraredTxPinExtPA7;
    } else {
        app->active_tx_pin = FuriHalInfraredTxPinInternal;
    }

    furi_hal_infrared_set_tx_output(app->active_tx_pin);
}

static const char* infrared_morse_beacon_get_code(char ch) {
    if((ch >= 'a') && (ch <= 'z')) ch -= ('a' - 'A');

    for(size_t i = 0; i < COUNT_OF(kMorseTable); i++) {
        if(kMorseTable[i].ch == ch) return kMorseTable[i].code;
    }

    return NULL;
}

static void infrared_morse_beacon_append_timing(
    InfraredMorseBeaconApp* app,
    bool level,
    uint32_t duration_us) {
    if(duration_us == 0) return;

    if(app->timings_count > 0 && app->levels[app->timings_count - 1] == level) {
        app->durations[app->timings_count - 1] += duration_us;
        return;
    }

    if(app->timings_count < COUNT_OF(app->durations)) {
        app->levels[app->timings_count] = level;
        app->durations[app->timings_count] = duration_us;
        app->timings_count++;
    }
}

static void infrared_morse_beacon_build_timings(InfraredMorseBeaconApp* app, const char* message) {
    app->timings_count = 0;

    const size_t len = strlen(message);
    for(size_t i = 0; i < len; i++) {
        const char ch = message[i];
        if(ch == ' ') continue;

        const char* code = infrared_morse_beacon_get_code(ch);
        if(!code) continue;

        for(size_t j = 0; code[j] != '\0'; j++) {
            const uint32_t mark_units = (code[j] == '-') ? 3 : 1;
            infrared_morse_beacon_append_timing(app, true, mark_units * kMorseUnitUs);

            if(code[j + 1] != '\0') {
                infrared_morse_beacon_append_timing(app, false, kMorseUnitUs);
            }
        }

        size_t next = i + 1;
        while((next < len) && (message[next] == ' ')) {
            next++;
        }

        if(next < len) {
            const uint32_t gap_units = (next > i + 1) ? 7 : 3;
            infrared_morse_beacon_append_timing(app, false, gap_units * kMorseUnitUs);
        }
    }

    if(app->timings_count == 0) {
        infrared_morse_beacon_append_timing(app, true, kMorseUnitUs);
    }
}

static FuriHalInfraredTxGetDataState infrared_morse_beacon_tx_data_isr(
    void* context,
    uint32_t* duration,
    bool* level) {
    InfraredMorseBeaconApp* app = context;

    if(app->tx_index >= app->timings_count) {
        *duration = kMorseUnitUs;
        *level = false;
        return FuriHalInfraredTxGetDataStateLastDone;
    }

    *duration = app->durations[app->tx_index];
    *level = app->levels[app->tx_index];

    const bool last = (app->tx_index + 1) >= app->timings_count;
    app->tx_index++;

    return last ? FuriHalInfraredTxGetDataStateLastDone : FuriHalInfraredTxGetDataStateOk;
}

static void infrared_morse_beacon_send_once(InfraredMorseBeaconApp* app) {
    app->tx_index = 0;
    furi_hal_infrared_async_tx_set_data_isr_callback(infrared_morse_beacon_tx_data_isr, app);
    furi_hal_infrared_async_tx_start(kIrFreqHz, kIrDutyCycle);
    furi_hal_infrared_async_tx_wait_termination();
}

static void infrared_morse_beacon_draw_callback(Canvas* canvas, void* context) {
    InfraredMorseBeaconApp* app = context;
    const char* message = kPresets[app->preset_index];
    const char* route = infrared_morse_beacon_route_get_name(app->tx_route);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "IR Morse Beacon");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 24, "Msg:");
    canvas_draw_str(canvas, 30, 24, message);

    canvas_draw_str(canvas, 2, 36, "TX:");
    canvas_draw_str(canvas, 24, 36, route);

    canvas_draw_str(canvas, 2, 48, app->beacon_enabled ? "Status: TX loop" : "Status: Stopped");
    canvas_draw_str(canvas, 2, 58, "OK:Start Up/Down:Msg");
    canvas_draw_str(canvas, 2, 64, "Left/Right:TX Back:Exit");
}

static void infrared_morse_beacon_input_callback(InputEvent* input_event, void* context) {
    InfraredMorseBeaconApp* app = context;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}

int32_t infrared_morse_beacon_app(void* p) {
    UNUSED(p);

    InfraredMorseBeaconApp* app = malloc(sizeof(InfraredMorseBeaconApp));
    memset(app, 0, sizeof(*app));

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    app->preset_index = 0;
    app->tx_route = InfraredMorseTxRouteAuto;

    infrared_morse_beacon_apply_tx_route(app);
    infrared_morse_beacon_build_timings(app, kPresets[app->preset_index]);

    view_port_draw_callback_set(app->view_port, infrared_morse_beacon_draw_callback, app);
    view_port_input_callback_set(app->view_port, infrared_morse_beacon_input_callback, app);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, app->view_port, GuiLayerFullscreen);

    bool running = true;
    while(running) {
        InputEvent event;
        if(furi_message_queue_get(app->event_queue, &event, 50) == FuriStatusOk) {
            if(event.type == InputTypeShort || event.type == InputTypePress) {
                switch(event.key) {
                case InputKeyBack:
                    running = false;
                    break;
                case InputKeyOk:
                    app->beacon_enabled = !app->beacon_enabled;
                    break;
                case InputKeyUp:
                    app->preset_index = (app->preset_index + 1) % COUNT_OF(kPresets);
                    infrared_morse_beacon_build_timings(app, kPresets[app->preset_index]);
                    break;
                case InputKeyDown:
                    app->preset_index = (app->preset_index + COUNT_OF(kPresets) - 1) %
                                        COUNT_OF(kPresets);
                    infrared_morse_beacon_build_timings(app, kPresets[app->preset_index]);
                    break;
                case InputKeyLeft:
                    app->tx_route = (app->tx_route + InfraredMorseTxRouteCount - 1) %
                                    InfraredMorseTxRouteCount;
                    infrared_morse_beacon_apply_tx_route(app);
                    break;
                case InputKeyRight:
                    app->tx_route = (app->tx_route + 1) % InfraredMorseTxRouteCount;
                    infrared_morse_beacon_apply_tx_route(app);
                    break;
                default:
                    break;
                }
            }
        }

        if(app->beacon_enabled &&
           (furi_get_tick() - app->last_tx_tick >= furi_ms_to_ticks(kRepeatDelayMs))) {
            app->last_tx_tick = furi_get_tick();
            infrared_morse_beacon_send_once(app);
        }

        view_port_update(app->view_port);
    }

    if(furi_hal_infrared_is_busy()) {
        furi_hal_infrared_async_tx_stop();
    }

    gui_remove_view_port(gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}
