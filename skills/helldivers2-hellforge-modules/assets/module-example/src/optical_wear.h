#pragma once

#include "cloak_energy.h"
#include <string.h>
#include "optical_effect.h"
#include <math.h>

static CloakEnergy optical_energy;
static HfRef worn_resource;

static uintptr_t optical_ptr(const HfApi* api, uintptr_t address)
{
    uintptr_t value;
    return api->read(address, &value, sizeof(value)) || value < 0x10000 ? 0 : value;
}

/* The game's entity-to-record map, as used by HellForge's equipment actions. */
static bool optical_index(const HfApi* api, uintptr_t map, uint32_t id, uint32_t* index)
{
    struct { uintptr_t buckets; uint32_t capacity, empty, multiplier; } table;
    if (!id || api->read(map, &table, 20) || table.buckets < 0x10000 ||
        !table.capacity || table.capacity > 1048576)
        return false;
    uint32_t first = id * table.multiplier;
    for (uint32_t i = 0; i < table.capacity && i < 4096; ++i) {
        uint32_t entry[2];
        uintptr_t slot = table.buckets + (uintptr_t)((first + i) % table.capacity) * sizeof(entry);
        if (api->read(slot, entry, sizeof(entry)) || entry[0] == table.empty)
            break;
        if (entry[0] == id) {
            *index = entry[1];
            return true;
        }
    }
    return false;
}

static uint32_t optical_peer_avatar(HfMod* mod, uintptr_t game, uint64_t peer)
{
    const HfApi* api = HF_API(mod);
    uintptr_t players = optical_ptr(api, game + 0x3326468u);
    uint32_t count;
    if (!players || !peer || api->read(players + 0x84, &count, sizeof(count)) || count > 4)
        return 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t owner;
        uint32_t life, network;
        if (!api->read(players + 0x2C8 + i * 0x38, &owner, sizeof(owner)) && owner == peer &&
            !api->read(players + 0x2E0 + i * 0x38, &life, sizeof(life)) && life == 3 &&
            !api->read(players + 0x3A8 + i * 0x20, &network, sizeof(network)) && network < 0x7FFF) {
            const uint32_t entity = api->entity_of_network(mod, network);
            return entity == UINT32_MAX ? 0 : entity;
        }
    }
    return 0;
}

static uint32_t optical_avatar(HfMod* mod, uintptr_t game)
{
    const HfApi* api = HF_API(mod);
    const uintptr_t peer_record = optical_ptr(api, game + 0x347CEF0u);
    uint64_t peer = 0;
    if (!peer_record || api->read(peer_record + 0xB398, &peer, sizeof(peer))) return 0;
    return optical_peer_avatar(mod, game, peer);
}

static uint32_t optical_entity_unit(const HfApi* api, uintptr_t entities, uint32_t entity, HfRef* resource)
{
    uint32_t index;
    struct { HfRef resource; uint32_t entity, unit; } descriptor;
    if (!entities || !optical_index(api, entities + 0xF1AEB0, entity, &index) || index >= 0x100000 ||
        api->read(entities + 0xF32F18 + (uintptr_t)index * 24, &descriptor, sizeof(descriptor)) ||
        descriptor.entity != entity)
        return 0;
    if (resource) *resource = descriptor.resource;
    return descriptor.unit;
}

/* The same root scene transform already used by the verified SEAF landing path. */
static bool optical_unit_position(const HfApi* api, uint32_t unit, float position[3])
{
    const uintptr_t exe = api->module_base("helldivers2.exe", NULL);
    const uintptr_t registry = optical_ptr(api, exe + 0x1A100F0u);
    const uint32_t index = unit & 0x3FFFFFu;
    uint32_t count = 0, stored = 0;
    uint8_t generation = 0;
    if (!unit || !exe || !registry || api->read(registry + 0x98, &count, 4) || index >= count)
        return false;
    const uintptr_t generations = optical_ptr(api, registry + 0xA0);
    const uintptr_t objects = optical_ptr(api, registry + 0x88);
    const uintptr_t object = optical_ptr(api, objects + (uintptr_t)index * 8);
    if (!generations || !objects || !object ||
        api->read(generations + index, &generation, 1) || generation != ((unit >> 22) & 0xFF) ||
        api->read(object + 8, &stored, 4) || stored != unit) return false;
    const uintptr_t vtable = optical_ptr(api, object);
    const uintptr_t method = optical_ptr(api, vtable + 0xE8);
    uintptr_t graph = 0;
    if (method == exe + 0x2BD870u && api->code_matches(method, "48 8D 41 60 C3"))
        graph = object + 0x60;
    else if (vtable == exe + 0x16770E0u && method == exe + 0x2CAC50u &&
        api->code_matches(method, "48 8D 41 48 C3")) graph = object + 0x48;
    const uintptr_t poses = graph ? optical_ptr(api, graph + 0x28) : 0;
    if (!poses || api->read(poses + 0x30, position, 12)) return false;
    for (unsigned i = 0; i < 3; ++i)
        if (!isfinite(position[i]) || fabsf(position[i]) > 100000.0f) return false;
    return true;
}

static bool optical_stationary(const HfApi* api, uint32_t unit, float dt)
{
    static uint32_t previous_unit;
    static float previous[3], elapsed, still;
    static bool sampled;
    if (!unit || unit != previous_unit || !(dt > 0 && dt <= 1)) {
        previous_unit = unit;
        sampled = false;
        elapsed = still = 0;
        return false;
    }
    elapsed += dt;
    if (elapsed < 0.1f) return still >= 0.35f;
    float position[3];
    if (!optical_unit_position(api, unit, position)) {
        sampled = false;
        elapsed = still = 0;
        return false;
    }
    float distance2 = 0;
    for (unsigned i = 0; i < 3; ++i) {
        const float d = position[i] - previous[i];
        distance2 += d * d;
    }
    /* A 0.15 m/s tolerance excludes walking, crawling and falling. */
    still = sampled && distance2 <= 0.0225f * elapsed * elapsed ? still + elapsed : 0;
    memcpy(previous, position, sizeof(previous));
    sampled = true;
    elapsed = 0;
    return still >= 0.35f;
}

#include "module_pickup.h"

static uint32_t optical_pack_worn(const HfApi* api, uintptr_t game, uint32_t avatar)
{
    (void)api;
    (void)game;
    return avatar && module_state.wearer == avatar ? module_state.token : 0;
}

static void optical_track(HfMod* mod, float dt, void* user)
{
    (void)user;
    const HfApi* api = HF_API(mod);
    module_pickup_tick(mod, dt);
    uint32_t avatar = 0, pack = 0, unit = 0;
    if (api->game_state(mod) == HF_STATE_MISSION) {
        uintptr_t game = api->module_base("game.dll", NULL);
        if (game && (avatar = optical_avatar(mod, game)) != 0) {
            pack = optical_pack_worn(api, game, avatar);
            if (pack) {
                unit = optical_entity_unit(api, optical_ptr(api, game + 0x346BF98u), avatar, NULL);
                if (!unit || !optical_unit_alive || !optical_unit_alive(unit))
                    pack = 0;
            }
        }
    }
    atomic_store_explicit(&optical_equipped_unit, pack ? unit : 0, memory_order_relaxed);
    if (optical_energy.pack != pack)
        api->log(mod, "Optical module wearer: %u, identity token: %u", pack ? avatar : 0, pack);
    const bool key = optical_key_down();
    if (optical_energy.active && (!unit || unit != optical_hidden ||
        optical_energy.wearer != avatar || optical_energy.pack != pack)) {
        optical_effect_stop(mod);
        optical_energy.active = false;
        optical_energy.key_was_down = key;
    }
    bool was_active = optical_energy.active;
    const bool stationary = optical_stationary(api, pack ? unit : 0, dt);
    cloak_energy_step(&optical_energy, pack ? avatar : 0, pack, key, dt, stationary);
    if (was_active && !optical_energy.active)
        optical_effect_stop(mod);
    if (!was_active && optical_energy.active && !optical_effect_start(mod, avatar, unit)) {
        optical_energy.active = false;
        api->log(mod, "Optical cloak activation refused: native effect hooks are unavailable.");
    }
    if (was_active != optical_energy.active)
        api->log(mod, "Optical cloak %s: avatar %u, unit %u, energy %.0f%%, key %c",
            optical_energy.active ? "on" : "off", avatar, unit,
            (double)(optical_energy.charge * 100.0f), optical_key);
    /* The prototype is deliberately solo-only. Never send cloak claims that
     * other peers cannot validate against a native backpack slot. */
}

static void optical_mission_end(HfMod* mod, const HfStateEvent* event, void* user)
{
    (void)event;
    (void)user;
    optical_effect_stop(mod);
    atomic_store_explicit(&optical_equipped_unit, 0, memory_order_relaxed);
    optical_energy = (CloakEnergy){0};
    module_clear(&module_state);
    AcquireSRWLockExclusive(&optical_links_lock);
    optical_links_count = 0;
    ReleaseSRWLockExclusive(&optical_links_lock);
}

static void optical_track_install(HfMod* mod, HfRef resource, HfTickCallback tick)
{
    const HfApi* api = HF_API(mod);
    if (!api->build_id || !api->module_base || !api->read || !api->game_state ||
        !api->entity_of_network || !api->on_game_thread || !api->on ||
        !api->is_authority || !api->session ||
        strcmp(api->build_id(), "25480438"))
        return;
    worn_resource = resource;
    optical_key = optical_read_key(api, mod);
    api->on(mod, HF_EVENT_GAME_LOADED, optical_bind_visual, NULL);
    api->on(mod, HF_EVENT_GAME_LOADED, module_bind_pickup, NULL);
    hf_on_mission_end(mod, optical_mission_end, NULL, 0);
    api->on_game_thread(mod, tick, NULL);
}
