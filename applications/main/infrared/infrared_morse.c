#include "infrared_morse.h"

#include <string.h>

typedef struct {
    char ch;
    const char* code;
} InfraredMorseMap;

static const InfraredMorseMap infrared_morse_map[] = {
    {'A', ".-"},   {'B', "-..."}, {'C', "-.-."}, {'D', "-.."},  {'E', "."},
    {'F', "..-."}, {'G', "--."},  {'H', "...."}, {'I', ".."},   {'J', ".---"},
    {'K', "-.-"},  {'L', ".-.."}, {'M', "--"},   {'N', "-."},   {'O', "---"},
    {'P', ".--."}, {'Q', "--.-"}, {'R', ".-."},  {'S', "..."},  {'T', "-"},
    {'U', "..-"},  {'V', "...-"}, {'W', ".--"},  {'X', "-..-"}, {'Y', "-.--"},
    {'Z', "--.."}, {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
    {'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."}, {'8', "---.."},
    {'9', "----."},
};

static char infrared_morse_to_upper(char ch) {
    if((ch >= 'a') && (ch <= 'z')) {
        return ch - ('a' - 'A');
    }
    return ch;
}

static const char* infrared_morse_get_code(char ch) {
    ch = infrared_morse_to_upper(ch);
    const size_t map_size = sizeof(infrared_morse_map) / sizeof(infrared_morse_map[0]);
    for(size_t i = 0; i < map_size; i++) {
        if(infrared_morse_map[i].ch == ch) {
            return infrared_morse_map[i].code;
        }
    }
    return NULL;
}

static bool infrared_morse_append_timing(
    bool level,
    uint32_t duration,
    uint32_t* out_timings,
    bool* level_state,
    bool* level_valid,
    size_t capacity,
    size_t* size) {
    if(duration == 0) return true;

    if(*level_valid && (*level_state == level)) {
        out_timings[*size - 1] += duration;
        return true;
    }

    if(*size >= capacity) {
        return false;
    }

    out_timings[*size] = duration;
    (*size)++;
    *level_state = level;
    *level_valid = true;
    return true;
}

bool infrared_morse_encode(
    const char* text,
    uint8_t wpm,
    uint32_t* out_timings,
    size_t out_capacity,
    size_t* out_size) {
    if(!text || !out_timings || !out_size || !out_capacity) {
        return false;
    }

    if(wpm < 5) wpm = 5;
    if(wpm > 40) wpm = 40;

    const uint32_t unit_us = 1200000U / wpm;
    const size_t text_len = strlen(text);
    size_t size = 0;
    bool level_state = false;
    bool level_valid = false;

    for(size_t i = 0; i < text_len; i++) {
        const char ch = text[i];

        if(ch == ' ') {
            continue;
        }

        const char* code = infrared_morse_get_code(ch);
        if(!code) {
            continue;
        }

        for(size_t j = 0; code[j] != '\0'; j++) {
            const uint32_t mark_units = (code[j] == '-') ? 3U : 1U;
            if(!infrared_morse_append_timing(
                   true,
                   mark_units * unit_us,
                   out_timings,
                   &level_state,
                   &level_valid,
                   out_capacity,
                   &size)) {
                return false;
            }

            if(code[j + 1] != '\0') {
                if(!infrared_morse_append_timing(
                       false,
                       unit_us,
                       out_timings,
                       &level_state,
                       &level_valid,
                       out_capacity,
                       &size)) {
                    return false;
                }
            }
        }

        size_t next = i + 1;
        while((next < text_len) && text[next] == ' ') {
            next++;
        }

        if(next < text_len) {
            const uint32_t gap_units = (next > i + 1) ? 7U : 3U;
            if(!infrared_morse_append_timing(
                   false,
                   gap_units * unit_us,
                   out_timings,
                   &level_state,
                   &level_valid,
                   out_capacity,
                   &size)) {
                return false;
            }
        }
    }

    if(size == 0) {
        if(out_capacity < 1) {
            return false;
        }
        out_timings[0] = unit_us;
        size = 1;
    }

    *out_size = size;
    return true;
}
