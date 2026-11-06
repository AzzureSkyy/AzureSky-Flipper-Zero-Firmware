#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

bool morse_beacon_encode(
    const char* text,
    uint8_t wpm,
    uint32_t* out_timings,
    size_t out_capacity,
    size_t* out_size);
