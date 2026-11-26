#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct ModuleState {
    uint32_t wearer, token;
    uint32_t pending_wearer, pending_pickup;
    float pending_seconds;
    bool requested;
} ModuleState;

static inline bool module_begin(ModuleState* state, uint32_t wearer, uint32_t pickup)
{
    if (!wearer || !pickup || state->token || state->pending_pickup) return false;
    state->pending_wearer = wearer;
    state->pending_pickup = pickup;
    state->pending_seconds = 0;
    state->requested = false;
    return true;
}

static inline void module_clear(ModuleState* state)
{
    *state = (ModuleState){0};
}

/* A consumed entity handle is an identity token, never a live unit pointer. */
static inline bool module_commit(ModuleState* state, uint32_t wearer)
{
    if (!state->pending_pickup || !state->requested || state->pending_wearer != wearer)
        return false;
    state->wearer = wearer;
    state->token = state->pending_pickup;
    state->pending_pickup = state->pending_wearer = 0;
    state->pending_seconds = 0;
    state->requested = false;
    return true;
}
