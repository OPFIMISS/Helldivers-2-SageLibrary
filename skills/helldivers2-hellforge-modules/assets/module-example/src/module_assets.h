#pragma once

/* SDK 16's load_package accepts catalogue packages only. These vanilla packages
 * were verified in bundles.nxa; request them through the same native system. */
typedef void (*ModulePackageRequest)(void*, const uint64_t*, uint32_t);
static ModulePackageRequest module_request_packages;
static const uint64_t module_packages[] = {
    0x46E418DD4A6542C4ull, /* stalker material/shader dependencies for optical presentation */
    0x18235E0C9EC0E636ull  /* shared stalker textures, including the cloak mask */
};
enum { MODULE_PACKAGE_COUNT = sizeof(module_packages) / sizeof(*module_packages) };
static uintptr_t module_loaded_packages[MODULE_PACKAGE_COUNT];
static bool module_packages_requested, module_assets_ready, module_assets_failed;
static ULONGLONG module_assets_since;
static float module_assets_poll;

static bool module_package_loaded(const HfApi* api, uintptr_t package)
{
    uint32_t count = 0;
    const uintptr_t parts = optical_ptr(api, package + 0x20);
    if (!parts || api->read(package + 0x18, &count, 4) || !count || count > 64) return false;
    for (uint32_t i = 0; i < count; ++i) {
        const uintptr_t part = optical_ptr(api, parts + i * 8u);
        uint32_t state = 0;
        if (!part || api->read(part + 0x1C, &state, 4) || state != 4) return false;
    }
    return true;
}

static void module_assets_tick(HfMod* mod, uintptr_t game, float dt)
{
    if (module_assets_ready || module_assets_failed || !module_request_packages) return;
    module_assets_poll -= dt;
    if (module_assets_poll > 0) return;
    module_assets_poll = 0.5f;
    const HfApi* api = HF_API(mod);
    if (!module_packages_requested) {
        const uintptr_t system = optical_ptr(api, game + 0x347CEB0u);
        const uintptr_t map = optical_ptr(api, system);
        uint32_t capacity = 0, used = 0;
        if (!system || !map || api->read(system + 8, &capacity, 4) ||
            capacity < 16 || capacity > 65536) return;
        /* The native reference map has no overflow path. */
        for (uint32_t i = 0; i < capacity; ++i) {
            uint64_t key = 0;
            if (api->read(map + (uintptr_t)i * 16, &key, 8)) return;
            used += key != 0;
        }
        if ((used + MODULE_PACKAGE_COUNT) * 4u >= capacity * 3u) {
            module_assets_failed = true;
            api->log(mod, "Module: package reference map has insufficient room");
            return;
        }
        module_request_packages((void*)system, module_packages, MODULE_PACKAGE_COUNT);
        module_packages_requested = true;
        module_assets_since = GetTickCount64();
    }
    const uintptr_t exe = api->module_base("helldivers2.exe", NULL);
    const uintptr_t engine = optical_ptr(api, exe + 0x1A10208u);
    const uintptr_t manager = optical_ptr(api, engine + 0x400);
    const uintptr_t resources = optical_ptr(api, manager + 0x208);
    const uintptr_t list = optical_ptr(api, resources + 0x88);
    uint32_t count = 0;
    if (!exe || !engine || !manager || !resources || !list ||
        api->read(resources + 0x80, &count, 4) || count > 8192) return;
    bool missing = false;
    for (unsigned j = 0; j < MODULE_PACKAGE_COUNT; ++j) missing |= module_loaded_packages[j] == 0;
    if (missing)
        for (uint32_t i = 0; i < count; ++i) {
            const uintptr_t package = optical_ptr(api, list + i * 8u);
            uint64_t id = 0;
            if (!package || api->read(package + 0x10, &id, 8)) continue;
            for (unsigned j = 0; j < MODULE_PACKAGE_COUNT; ++j)
                if (id == module_packages[j]) module_loaded_packages[j] = package;
        }
    bool ready = true;
    for (unsigned j = 0; j < MODULE_PACKAGE_COUNT; ++j)
        ready &= module_loaded_packages[j] && module_package_loaded(api, module_loaded_packages[j]);
    if (ready) {
        module_assets_ready = true;
        api->log(mod, "Module: optical material/shader/texture dependencies resident");
    } else if (GetTickCount64() - module_assets_since > 90000) {
        module_assets_failed = true;
        api->log(mod, "Module: native packages did not become resident within 90 seconds");
    }
}
