#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t wearer;
    uint32_t pack;
    float charge;
    bool active;
    bool key_was_down;
} CloakEnergy;

/* Full recharge: 20 seconds. Cloak: 12 seconds moving, 24 seconds stationary. */
static inline void cloak_energy_step(CloakEnergy* state, uint32_t wearer,
    uint32_t pack, bool key_down, float seconds, bool stationary)
{
    const bool pressed = key_down && !state->key_was_down;
    if (!wearer || !pack || state->wearer != wearer || state->pack != pack) {
        *state = (CloakEnergy){.wearer = wearer, .pack = pack, .key_was_down = key_down};
        return;
    }
    state->key_was_down = key_down;
    if (!(seconds > 0.0f && seconds <= 1.0f))
        seconds = 0.0f;

    if (state->active) {
        state->charge -= seconds / (stationary ? 24.0f : 12.0f);
        if (state->charge <= 0.0f) {
            state->charge = 0.0f;
            state->active = false;
            return;
        }
    } else {
        state->charge += seconds / 20.0f;
        if (state->charge > 1.0f)
            state->charge = 1.0f;
    }

    if (pressed && (state->active || state->charge >= 0.30f))
        state->active = !state->active;
}
