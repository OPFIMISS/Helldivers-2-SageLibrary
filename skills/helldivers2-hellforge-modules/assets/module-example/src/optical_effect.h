#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>
#include "hellforge_components.h"

typedef void (*OpticalUnitVisibility)(uint32_t, uint8_t, uint8_t);
typedef uint8_t (*OpticalUnitAlive)(uint32_t);
typedef uint8_t (*OpticalValidTarget)(void*, uint32_t, void*);
typedef void (*OpticalSetTarget)(void*, const uint8_t*);
typedef float* (*OpticalTargetPosition)(float*, void*, const float*);
typedef uint64_t (*OpticalTags)(void*, uint32_t);

static OpticalUnitVisibility optical_visibility;
static OpticalUnitAlive optical_unit_alive;
static OpticalValidTarget optical_original_target;
static OpticalSetTarget optical_set_target;
static OpticalTargetPosition optical_original_position;
static OpticalTags optical_original_tags;
static uintptr_t optical_scan_return;
static _Atomic uint32_t optical_hidden_avatar;
static _Atomic uint32_t optical_remote_avatars[4];
static uint32_t optical_hidden;
static bool optical_visual_ready;
static int optical_key = 'H';

#include "optical_links.h"

static int optical_read_key(const HfApi* api, HfMod* mod)
{
    const char* folder = api->mod_folder ? api->mod_folder(mod) : NULL;
    wchar_t path[32768];
    const int len = folder ? MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, folder, -1,
        path, (int)(sizeof(path) / sizeof(path[0]))) : 0;
    const wchar_t suffix[] = L"\\optical.ini";
    if (!len || (size_t)(len - 1) + sizeof(suffix) / sizeof(*suffix) > sizeof(path) / sizeof(*path))
        return 'H';
    memcpy(path + len - 1, suffix, sizeof(suffix));
    wchar_t key[8];
    GetPrivateProfileStringW(L"optical", L"key", L"H", key, sizeof(key) / sizeof(key[0]), path);
    if (key[1] != 0)
        return 'H';
    int ch = key[0];
    if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
    return (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ? ch : 'H';
}

static bool optical_key_down(void)
{
    DWORD foreground = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foreground);
    return foreground == GetCurrentProcessId() && (GetAsyncKeyState(optical_key) & 0x8000);
}

static bool optical_is_hidden(uint32_t entity)
{
    if (!entity) return false;
    if (entity == atomic_load_explicit(&optical_hidden_avatar, memory_order_relaxed)) return true;
    for (unsigned i = 0; i < 4; ++i)
        if (entity == atomic_load_explicit(&optical_remote_avatars[i], memory_order_relaxed)) return true;
    return false;
}

static uint64_t optical_perception_tags(void* context, uint32_t entity)
{
    const uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    const uint64_t flags = optical_original_tags(context, entity);
    /* Only the AI candidate scan sees this flag; the entity's real tags stay intact. */
    return caller == optical_scan_return && optical_is_hidden(entity) ? flags | 0x02000000ull : flags;
}

static uint8_t optical_valid_target(void* context, uint32_t entity, void* info)
{
    if (optical_is_hidden(entity))
        return 0;
    return optical_original_target(context, entity, info);
}

static float* optical_target_position(float* out, void* context, const float* self)
{
    if (context) {
        uint8_t* behaviour;
        memcpy(&behaviour, (const uint8_t*)context + sizeof(void*), sizeof(behaviour));
        if (behaviour && behaviour[0x70]) {
            uint32_t target;
            memcpy(&target, behaviour + 0x10, sizeof(target));
            if (optical_is_hidden(target))
                optical_set_target(context, NULL);
        }
    }
    return optical_original_position(out, context, self);
}

static bool optical_effect_start(HfMod* mod, uint32_t avatar, uint32_t unit)
{
    (void)mod;
    if (!optical_visual_ready || !optical_visibility || !optical_unit_alive || !optical_original_target ||
        !optical_original_position || !optical_set_target || !optical_original_tags ||
        !avatar || !unit || !optical_unit_alive(unit))
        return false;
    optical_hidden = unit;
    atomic_store_explicit(&optical_hidden_avatar, avatar, memory_order_relaxed);
    atomic_store_explicit(&optical_visual_roots[0], unit, memory_order_relaxed);
    return true;
}

static void optical_effect_stop(HfMod* mod)
{
    (void)mod;
    atomic_store_explicit(&optical_hidden_avatar, 0, memory_order_relaxed);
    atomic_store_explicit(&optical_visual_roots[0], 0, memory_order_relaxed);
    if (!optical_hidden)
        return;
    optical_hidden = 0;
}

static void optical_bind_visual(HfMod* mod, HfEvent event, void* user)
{
    (void)event;
    (void)user;
    const HfApi* api = HF_API(mod);
    const uintptr_t exe = api->module_base("helldivers2.exe", NULL);
    const uintptr_t game = api->module_base("game.dll", NULL);
    if (exe && api->code_matches &&
        api->code_matches(exe + 0x205680u, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 41 0F B6 F0 8B FA 8B D9") &&
        api->code_matches(exe + 0x1FEEB0u, "48 83 EC 28 E8 07 EA E9 FF 48 85 C0 0F 95 C0 48 83 C4 28 C3")) {
        optical_visibility = (OpticalUnitVisibility)(exe + 0x205680u);
        optical_unit_alive = (OpticalUnitAlive)(exe + 0x1FEEB0u);
    } else {
        api->log(mod, "Optical cloak unavailable: unit visibility functions did not match the reviewed build.");
    }
    if (optical_visibility && api->hook_ex &&
        api->code_matches(exe + 0x1AF240u, "48 8B C4 48 89 58 08 48 89 68 10 48 89 70 18 48 89 78 20 41 56") &&
        api->code_matches(exe + 0x1AF780u, "40 53 48 83 EC 20 FF 81 6C 09 23 00 48 8B DA") &&
        !api->hook_ex(mod, exe + 0x1AF240u, (void*)optical_unit_linked, (void**)&optical_link_original, 0) &&
        !api->hook_ex(mod, exe + 0x1AF780u, (void*)optical_unit_unlinked, (void**)&optical_unlink_original, 0) &&
        !api->hook_ex(mod, exe + 0x205680u, (void*)optical_unit_visible, (void**)&optical_visibility, 0)) {
        optical_visual_ready = true;
        api->log(mod, "Optical: character attachments and persistent visibility hooks ready");
    } else {
        api->log(mod, "Optical: character attachment visibility hooks unavailable");
    }
    if (optical_visibility && game && api->hook_ex &&
        api->code_matches(game + 0x4A9710u, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20") &&
        api->code_matches(game + 0x88739Bu, "E8 70 23 C2 FF 48 A9 00 00 00 06") &&
        !api->hook_ex(mod, game + 0x4A9710u, (void*)optical_perception_tags, (void**)&optical_original_tags, 0) &&
        api->code_matches(game + 0x8858A0u, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56") &&
        !api->hook_ex(mod, game + 0x8858A0u, (void*)optical_valid_target, (void**)&optical_original_target, 0)) {
        optical_scan_return = game + 0x8873A0u;
        // Clear cached targets through the native setter for behaviours using this aim point.
        if (api->code_matches(game + 0x4AF4E0u, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 55 41 56 41 57 4C 8B 41 08") &&
            api->code_matches(game + 0x4B0C40u, "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 55 41 56 41 57 48 83 EC 40 48 8B 5A 08")) {
            optical_set_target = (OpticalSetTarget)(game + 0x4AF4E0u);
            if (!api->hook_ex(mod, game + 0x4B0C40u, (void*)optical_target_position, (void**)&optical_original_position, 0))
                return;
        }
        api->log(mod, "Optical cloak unavailable: the cached aim target hook was not installed.");
    } else {
        api->log(mod, "Optical cloak unavailable: the target validity hook was not installed.");
    }
}
