#pragma once

#include "module_state.h"

typedef void (*ModuleSwap)(void*, uint32_t, uint32_t, uint32_t, uint8_t);
typedef void (*ModuleRetire)(void*, uint32_t);
static ModuleSwap module_swap_next;
static ModuleRetire module_retire;
static HfMod* module_mod;
static uintptr_t module_game;
static ModuleState module_state;
static bool module_pickup_ready;

static bool module_solo(HfMod* mod)
{
    HfSession session = {.role = HF_SESSION_JOINER};
    HF_API(mod)->session(mod, &session);
    uint32_t players = 5;
    const HfApi* api = HF_API(mod);
    const uintptr_t manager = module_game ? optical_ptr(api, module_game + 0x3326468u) : 0;
    return session.role == HF_SESSION_SOLO && manager &&
        !api->read(manager + 0x84, &players, sizeof(players)) && players <= 1;
}

static void module_swap(void* manager, uint32_t wearer, uint32_t item,
    uint32_t slot, uint8_t drop)
{
    const HfApi* api = HF_API(module_mod);
    HfRef resource = 0;
    const uintptr_t entities = optical_ptr(api, module_game + 0x346BF98u);
    optical_entity_unit(api, entities, item, &resource);
    if (resource != worn_resource) {
        module_swap_next(manager, wearer, item, slot, drop);
        return;
    }
    /* Every path refuses to put our device in Inventory. Do not depend on a
     * caller return address: another mod's hook chain may add a stack frame. */
    if (slot != 6 ||
        !module_solo(module_mod) || api->game_state(module_mod) != HF_STATE_MISSION ||
        wearer != optical_avatar(module_mod, module_game) || !api->is_authority(module_mod, item))
        return;
    const uint32_t unit = optical_entity_unit(api, entities, wearer, NULL);
    if (!unit || !optical_unit_alive || !optical_unit_alive(unit)) return;
    if (module_begin(&module_state, wearer, item))
        api->log(module_mod, "Module pickup accepted: avatar %u, device %u; native backpack untouched", wearer, item);
}

static void module_pickup_tick(HfMod* mod, float dt)
{
    const HfApi* api = HF_API(mod);
    const uint32_t wearer = module_solo(mod) && api->game_state(mod) == HF_STATE_MISSION ?
        optical_avatar(mod, module_game) : 0;
    const uintptr_t entities = optical_ptr(api, module_game + 0x346BF98u);
    const uint32_t unit = optical_entity_unit(api, entities, wearer, NULL);
    if (!wearer || !unit || !optical_unit_alive || !optical_unit_alive(unit) ||
        (module_state.wearer && module_state.wearer != wearer) ||
        (module_state.pending_wearer && module_state.pending_wearer != wearer)) {
        module_clear(&module_state);
        return;
    }
    if (!module_state.pending_pickup) return;
    HfRef resource = 0;
    const uint32_t pickup = module_state.pending_pickup;
    const uint32_t pickup_unit = optical_entity_unit(api, entities, pickup, &resource);
    if (!module_state.requested) {
        if (entities && !pickup_unit) {
            /* Some interaction records already retire themselves on success. */
            module_state.requested = true;
            if (module_commit(&module_state, wearer))
                api->log(mod, "Optical module installed: %u; interaction retired device", wearer);
            return;
        }
        if (!module_retire || !entities || resource != worn_resource ||
            !api->is_authority(mod, pickup)) {
            api->log(mod, "Module pickup cancelled: device changed before native retirement");
            module_clear(&module_state);
            return;
        }
        /* Run after the interaction has unwound. FDC310 queues native entity
         * teardown (or sends its destroy RPC), never World.destroy_unit. */
        module_retire((void*)entities, pickup);
        module_state.requested = true;
        return;
    }
    if (!pickup_unit || !optical_unit_alive(pickup_unit)) {
        if (module_commit(&module_state, wearer))
            api->log(mod, "Optical module installed: %u; device retired, native backpack unchanged", wearer);
        return;
    }
    if (dt > 0 && dt <= 1) module_state.pending_seconds += dt;
    if (module_state.pending_seconds >= 5) {
        api->log(mod, "Module pickup cancelled: native retirement not confirmed within 5 seconds");
        module_clear(&module_state);
    }
}

static void module_bind_pickup(HfMod* mod, HfEvent event, void* user)
{
    (void)event;
    (void)user;
    const HfApi* api = HF_API(mod);
    module_mod = mod;
    module_game = api->module_base("game.dll", NULL);
    if (!module_game || !api->code_matches(module_game + 0x9AE030,
            "40 53 55 41 55 48 83 EC 40 48 89 7C 24 68 41 8B E8") ||
        !api->code_matches(module_game + 0x97DA94,
            "48 8B 0D 9D 8C 9A 02 41 B9 06 00 00 00 44 8B C6 C6 44 24 20 00 8B D3 E8 80 05 03 00 E9") ||
        !api->code_matches(module_game + 0xFDC310,
            "48 89 5C 24 08 57 48 83 EC 30 3B 15 4C 82 4A 02 8B DA 48 8B F9")) {
        api->log(mod, "Module disabled: reviewed pickup/retirement functions did not match build 25480438");
        return;
    }
    module_retire = (ModuleRetire)(module_game + 0xFDC310);
    if (api->hook_ex(mod, module_game + 0x9AE030, (void*)module_swap,
            (void**)&module_swap_next, 0)) {
        api->log(mod, "Module disabled: could not intercept native backpack installation");
        return;
    }
    module_pickup_ready = true;
    api->log(mod, "Module: single-player pickup interception and deferred native retirement ready");
}
