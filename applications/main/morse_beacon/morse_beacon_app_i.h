#pragma once

#include <furi.h>
#include <furi_hal_infrared.h>
#include <furi_hal_power.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/text_input.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/variable_item_list.h>

#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>

#include "scenes/morse_beacon_scene.h"
#include "morse_beacon_encoder.h"

#define MORSE_BEACON_TEXT_MAX_LENGTH 96
#define MORSE_BEACON_MAX_TIMINGS     1024
#define MORSE_BEACON_TEXT_STORE_SIZE 128

#define MORSE_BEACON_TEXT_DIR      EXT_PATH("subghz")
#define MORSE_BEACON_TEXT_PATH     MORSE_BEACON_TEXT_DIR "/morse_beacon_text.txt"
#define MORSE_BEACON_SAVES_DIR     MORSE_BEACON_TEXT_DIR "/morse_beacon"
#define MORSE_BEACON_SAVE_NAME_MAX 40

#define MORSE_BEACON_CARRIER_FREQUENCY 38000
#define MORSE_BEACON_DUTY_CYCLE        0.33f

typedef enum {
	MorseBeaconCustomEventTextEditDone = 100,
} MorseBeaconCustomEvent;

typedef enum {
	MorseBeaconViewTextInput,
	MorseBeaconViewDialogEx,
	MorseBeaconViewVariableList,
} MorseBeaconView;

typedef struct {
	bool is_running; /**< Whether Morse beacon loop is currently active. */
	bool is_otg_enabled; /**< Whether OTG power (external 5V) is enabled. */
	uint32_t last_start_tick;
	FuriHalInfraredTxPin tx_pin;
	uint8_t wpm;
} MorseBeaconState;

typedef struct {
	SceneManager* scene_manager;
	ViewDispatcher* view_dispatcher;

	Gui* gui;
	Storage* storage;
	DialogsApp* dialogs;
	NotificationApp* notifications;

	TextInput* text_input;
	DialogEx* dialog_ex;
	VariableItemList* var_item_list;

	char text_store[MORSE_BEACON_TEXT_STORE_SIZE + 1];
	char text[MORSE_BEACON_TEXT_MAX_LENGTH + 1];
	uint32_t timings[MORSE_BEACON_MAX_TIMINGS];
	size_t timings_count;
	volatile bool stop_requested; /**< Set to end Morse TX at next timing. */
	size_t tx_index; /**< Position in timings (ISR-owned while TX). */

	MorseBeaconState state;
} MorseBeaconApp;

void morse_beacon_text_store_set(MorseBeaconApp* app, const char* fmt, ...)
	_ATTRIBUTE((__format__(__printf__, 2, 3)));

void morse_beacon_text_input_callback(void* context);

void morse_beacon_show_error_message(MorseBeaconApp* app, const char* fmt, ...)
	_ATTRIBUTE((__format__(__printf__, 2, 3)));

void morse_beacon_set_tx_pin(MorseBeaconApp* app, FuriHalInfraredTxPin tx_pin);
void morse_beacon_enable_otg(MorseBeaconApp* app, bool enable);
void morse_beacon_save_settings(MorseBeaconApp* app);

bool morse_beacon_build_signal(MorseBeaconApp* app, const char* text, uint8_t wpm);
bool morse_beacon_load_text(MorseBeaconApp* app);
bool morse_beacon_load_text_from(MorseBeaconApp* app, const char* path);
bool morse_beacon_save_text(MorseBeaconApp* app);
bool morse_beacon_save_text_named(MorseBeaconApp* app, FuriString* saved_path);
void morse_beacon_start(MorseBeaconApp* app);
void morse_beacon_stop(MorseBeaconApp* app);
