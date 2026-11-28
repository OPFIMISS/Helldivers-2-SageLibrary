#pragma once

typedef struct OpticalLua OpticalLua;
typedef int (*OpticalLuaCall)(OpticalLua*, int, int, int);
typedef int (*OpticalLuaFunction)(OpticalLua*);
static OpticalLuaCall optical_lua_next;
static struct {
    int (*top)(OpticalLua*);
    void (*restore)(OpticalLua*, int);
    void (*get)(OpticalLua*, int, const char*);
    void (*set)(OpticalLua*, int, const char*);
    int (*type)(OpticalLua*, int);
    void (*push)(OpticalLua*, double);
    void (*closure)(OpticalLua*, OpticalLuaFunction, int);
    void (*boolean)(OpticalLua*, int);
    void* (*userdata)(OpticalLua*, int);
    int (*load)(OpticalLua*, const char*, size_t, const char*);
    const char* (*string)(OpticalLua*, int, size_t*);
    double (*number)(OpticalLua*, int);
    void (*pointer)(OpticalLua*, void*);
    void (*nil)(OpticalLua*);
} optical_lua;
static void* (*optical_lua_resolve)(uint32_t);
static _Atomic DWORD optical_gui_thread;
static atomic_flag optical_gui_gate = ATOMIC_FLAG_INIT;
static _Atomic uint32_t optical_gui_snapshot;
static HfMod* optical_gui_mod;
static _Atomic ULONGLONG optical_gui_next;
static bool optical_gui_tried, optical_gui_error_reported;
static _Thread_local bool optical_gui_busy;
static char* optical_gui_source;
static size_t optical_gui_source_length;
static const char optical_gui_name[] = "__liuli_optical_check";

static int rocket_lua_poll(OpticalLua* lua)
{
    optical_lua.push(lua,atomic_load(&rocket_ui_state));
    optical_lua.push(lua,atomic_load(&rocket_ui_count));
    const uint32_t unit=atomic_load(&rocket_ui_target_unit);
    if (unit) optical_lua.pointer(lua,(void*)((uintptr_t)unit<<2));
    else optical_lua.nil(lua);
    optical_lua.push(lua,atomic_load(&rocket_ui_distance));
    optical_lua.push(lua,atomic_load(&rocket_ui_keys));
    const uint32_t avatar_unit=rocket_avatar ? optical_entity_unit(HF_API(optical_gui_mod),
        (uintptr_t)hf_global_EntityManager(optical_gui_mod),rocket_avatar,NULL) : 0;
    if (avatar_unit) optical_lua.pointer(lua,(void*)((uintptr_t)avatar_unit<<2));
    else optical_lua.nil(lua);
    for (unsigned j=0;j<3;++j) optical_lua.push(lua,atomic_load(&rocket_ui_point[j]));
    optical_lua.push(lua,atomic_load(&rocket_ui_progress));
    return 10;
}

static int rocket_lua_observe(OpticalLua* lua)
{
    if (optical_lua.top(lua)<6) return 0;
    float view[6];
    for (unsigned i=0;i<6;++i) {
        if (optical_lua.type(lua,(int)i+1)!=3) return 0;
        view[i]=(float)optical_lua.number(lua,(int)i+1);
        if (!isfinite(view[i]) || fabsf(view[i])>100000) return 0;
    }
    if (!rocket_normalize(view+3)) return 0;
    for (unsigned i=0;i<6;++i) atomic_store(&rocket_camera[i],view[i]);
    atomic_store(&rocket_camera_at,GetTickCount64());
    if (optical_lua.top(lua)>=10) {
        const int type=optical_lua.type(lua,7);
        const uint32_t unit=(type==2 || type==7)?(uint32_t)((uintptr_t)optical_lua.userdata(lua,7)>>2):0;
        float point[3];
        for (unsigned i=0;i<3;++i) {
            if (optical_lua.type(lua,(int)i+8)!=3) return 0;
            point[i]=(float)optical_lua.number(lua,(int)i+8);
            if (!isfinite(point[i]) || fabsf(point[i])>100000) return 0;
        }
        if (unit && unit==atomic_load(&rocket_ui_target_unit)) {
            for (unsigned i=0;i<3;++i) atomic_store(&rocket_weak_position[i],point[i]);
            atomic_store(&rocket_weak_entity,atomic_load(&rocket_ui_target_entity));
            atomic_store(&rocket_weak_at,GetTickCount64());
        }
    }
    return 0;
}

static int rocket_lua_terminal_poll(OpticalLua* lua)
{
    optical_lua.push(lua,atomic_load(&rocket_tv_control));
    for (unsigned i=0;i<7;++i) optical_lua.push(lua,atomic_load(&rocket_tv_view[i]));
    optical_lua.push(lua,rocket_tv_exit_key);
    optical_lua.push(lua,rocket_guide_key);
    optical_lua.push(lua,atomic_load(&rocket_tv_speed_view));
    optical_lua.boolean(lua,atomic_load(&rocket_tv_boosted));
    return 12;
}

static int rocket_lua_terminal_ack(OpticalLua* lua)
{
    if (optical_lua.top(lua)<2 || optical_lua.type(lua,1)!=3 || optical_lua.type(lua,2)!=3) return 0;
    const uint32_t generation=(uint32_t)optical_lua.number(lua,1);
    if (generation && generation==atomic_load(&rocket_tv_control)) {
        if (optical_lua.number(lua,2)>0) atomic_store(&rocket_tv_camera_ack,GetTickCount64());
        else atomic_store(&rocket_tv_cancel,generation);
    }
    return 0;
}

static int rocket_lua_special_target(OpticalLua* lua)
{
    const int index=optical_lua.top(lua)>0?(int)optical_lua.number(lua,1)-1:-1;
    if (index<0 || index>=6) {
        optical_lua.push(lua,atomic_load(&rocket_sky_inflight));return 1;
    }
    optical_lua.push(lua,atomic_load(&rocket_sky_ui[index].count));
    for (unsigned j=0;j<3;++j) optical_lua.push(lua,atomic_load(&rocket_sky_ui[index].point[j]));
    return 4;
}

static int optical_lua_poll(OpticalLua* lua)
{
    const uint32_t snapshot = atomic_load_explicit(&optical_gui_snapshot, memory_order_relaxed);
    optical_lua.push(lua, (double)GetTickCount64());
    optical_lua.push(lua, (snapshot & 0x10000u) ? (snapshot & 0xFFFFu) / 10000.0 : -1.0);
    optical_lua.boolean(lua, (snapshot & 0x20000u) != 0);
    optical_lua.push(lua, (snapshot & 0x10000u) ?
        atomic_load_explicit(&optical_equipped_unit, memory_order_relaxed) : 0);
    for (unsigned i = 0; i < 5; ++i)
        optical_lua.push(lua, atomic_load_explicit(&optical_visual_roots[i], memory_order_relaxed));
    optical_lua.push(lua, atomic_load_explicit(&optical_visual_revision, memory_order_relaxed));
    optical_lua.boolean(lua, (snapshot & 0x40000u) != 0);
    return 11;
}

static int optical_lua_relation(OpticalLua* lua)
{
    const int type = optical_lua.type(lua, 1);
    const uint32_t unit = (type == 2 || type == 7) ?
        (uint32_t)((uintptr_t)optical_lua.userdata(lua, 1) >> 2) : 0;
    const uint32_t equipped = atomic_load_explicit(&optical_equipped_unit, memory_order_relaxed);
    bool hidden = optical_visual_hidden(unit), tracked = hidden || (unit && unit == equipped);
    if (!tracked && equipped) {
        AcquireSRWLockShared(&optical_links_lock);
        tracked = optical_descendant(unit, equipped);
        ReleaseSRWLockShared(&optical_links_lock);
    }
    /* Recover links made before our hook. This runs only during cache rebuilds. */
    if (!tracked && unit && optical_lua_resolve) {
        const HfApi* api = HF_API(optical_gui_mod);
        uint32_t child = unit;
        for (unsigned depth = 0; child && depth < 32; ++depth) {
            const uintptr_t object = (uintptr_t)optical_lua_resolve(child);
            uintptr_t parent = 0;
            uint32_t parent_id = 0;
            if (!object || api->read(object + 0x1D0, &parent, sizeof(parent)) || parent < 0x10000 ||
                api->read(parent + 8, &parent_id, sizeof(parent_id)) || !parent_id || parent_id == child ||
                !optical_unit_alive(parent_id)) break;
            optical_link_record(child, parent_id);
            hidden |= optical_visual_hidden(parent_id);
            tracked |= hidden || parent_id == equipped;
            if (tracked) break;
            child = parent_id;
        }
    }
    optical_lua.boolean(lua, hidden);
    optical_lua.boolean(lua, tracked);
    return 2;
}

static int optical_lua_notice(OpticalLua* lua)
{
    const char* message = optical_lua.string(lua, 1, NULL);
    if (message) HF_API(optical_gui_mod)->log(optical_gui_mod, "Optical view: %.400s", message);
    return 0;
}

static int optical_gui_call(OpticalLua* lua, int args, int results, int error)
{
    const DWORD owner = atomic_load_explicit(&optical_gui_thread, memory_order_relaxed);
    if (!optical_gui_busy && (!owner || GetCurrentThreadId() == owner) &&
        GetTickCount64() >= optical_gui_next &&
        !atomic_flag_test_and_set_explicit(&optical_gui_gate, memory_order_acquire)) {
        optical_gui_busy = true;
        const int top = optical_lua.top(lua);
        optical_lua.get(lua, -10002, "stingray");
        const bool engine = optical_lua.type(lua, -1) == 5;
        optical_lua.restore(lua, top);
        optical_lua.get(lua, -10002, "update");
        const bool update = optical_lua.type(lua, -1) == 6;
        optical_lua.restore(lua, top);
        if (!engine || !update) goto done;
        optical_gui_next = GetTickCount64() + 1000;
        optical_lua.get(lua, -10002, optical_gui_name);
        if (optical_lua.type(lua, -1) != 6) {
            optical_lua.restore(lua, top);
            static const struct { const char* name; OpticalLuaFunction fn; } callbacks[] = {
                {"__liuli_optical_poll", optical_lua_poll},
                {"__liuli_optical_relation", optical_lua_relation},
                {"__liuli_optical_notice", optical_lua_notice},
                {"__liuli_rockets_poll", rocket_lua_poll},
                {"__liuli_rockets_observe", rocket_lua_observe},
                {"__liuli_rocket_terminal_poll", rocket_lua_terminal_poll},
                {"__liuli_rocket_terminal_ack", rocket_lua_terminal_ack},
                {"__liuli_rocket_special_target", rocket_lua_special_target}
            };
            for (unsigned i = 0; i < sizeof(callbacks) / sizeof(callbacks[0]); ++i) {
                optical_lua.closure(lua, callbacks[i].fn, 0);
                optical_lua.set(lua, -10002, callbacks[i].name);
            }
            int failed = optical_lua.load(lua, optical_gui_source,
                optical_gui_source_length, "=field_support_view");
            if (!failed) failed = optical_lua_next(lua, 0, 0, 0);
            if (failed) goto failed;
            optical_lua.restore(lua, top);
            optical_lua.get(lua, -10002, optical_gui_name);
        }
        if (optical_lua.type(lua, -1) == 6) {
            atomic_store_explicit(&optical_gui_thread, GetCurrentThreadId(), memory_order_relaxed);
            if (optical_lua_next(lua, 0, 0, 0)) goto failed;
        }
        goto done;
failed:
        if (!optical_gui_error_reported) {
            const char* why = optical_lua.string(lua, -1, NULL);
            HF_API(optical_gui_mod)->log(optical_gui_mod, "Optical HUD: %s", why ? why : "Lua setup failed");
            optical_gui_error_reported = true;
        }
done:
        optical_lua.restore(lua, top);
        optical_gui_busy = false;
        atomic_flag_clear_explicit(&optical_gui_gate, memory_order_release);
    }
    return optical_lua_next(lua, args, results, error);
}

static bool optical_gui_read_file(HfMod* mod,const wchar_t* suffix,char** output,size_t* output_size)
{
    const char* folder = HF_API(mod)->mod_folder(mod);
    wchar_t path[32768];
    const int length = folder ? MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, folder, -1,
        path, sizeof(path) / sizeof(*path)) : 0;
    const size_t suffix_size=(wcslen(suffix)+1)*sizeof(wchar_t);
    if (!length || (size_t)length - 1 + suffix_size/sizeof(wchar_t) > sizeof(path) / sizeof(*path))
        return false;
    memcpy(path + length - 1, suffix, suffix_size);
    const HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    const DWORD size = GetFileSize(file, NULL);
    DWORD read = 0;
    char* source = size && size < 131072 ? HeapAlloc(GetProcessHeap(), 0, size) : NULL;
    const bool ok = source && ReadFile(file, source, size, &read, NULL) && read == size;
    CloseHandle(file);
    if (!ok) { if (source) HeapFree(GetProcessHeap(), 0, source); return false; }
    *output=source;
    *output_size=size;
    return true;
}

static bool optical_gui_read_source(HfMod* mod)
{
    char *optical=NULL,*rocket=NULL,*terminal=NULL;
    size_t a=0,b=0,c=0;
    if (!optical_gui_read_file(mod,L"\\optical_view.lua",&optical,&a) ||
        !optical_gui_read_file(mod,L"\\rocket_view.lua",&rocket,&b) ||
        !optical_gui_read_file(mod,L"\\rocket_terminal.lua",&terminal,&c)) {
        if (optical) HeapFree(GetProcessHeap(),0,optical);
        if (rocket) HeapFree(GetProcessHeap(),0,rocket);
        return false;
    }
    char* combined=HeapAlloc(GetProcessHeap(),0,a+b+c+20);
    if (combined) {
        memcpy(combined,optical,a);
        memcpy(combined+a,"\ndo\n",4);
        memcpy(combined+a+4,rocket,b);
        memcpy(combined+a+4+b,"\nend\ndo\n",8);
        memcpy(combined+a+12+b,terminal,c);
        memcpy(combined+a+12+b+c,"\nend\n",5);
        optical_gui_source=combined;
        optical_gui_source_length=a+b+c+17;
    }
    HeapFree(GetProcessHeap(),0,optical);
    HeapFree(GetProcessHeap(),0,rocket);
    HeapFree(GetProcessHeap(),0,terminal);
    return combined!=NULL;
}

static void optical_gui_tick(HfMod* mod, float dt, void* user)
{
    optical_track(mod, dt, user);
    const bool equipped = HF_API(mod)->game_state(mod) == HF_STATE_MISSION &&
        optical_energy.pack && optical_energy.wearer &&
        atomic_load_explicit(&optical_equipped_unit, memory_order_relaxed);
    const uint32_t snapshot = equipped ?
        0x10000u | (uint32_t)(optical_energy.charge * 10000.0f) |
        (optical_energy.active ? 0x20000u : 0) : 0;
    atomic_store_explicit(&optical_gui_snapshot, snapshot | (seaf_assets_ready ? 0x40000u : 0), memory_order_relaxed);
    if (optical_gui_tried || HF_API(mod)->game_state(mod) != HF_STATE_MISSION) return;
    HMODULE lua = GetModuleHandleW(L"lua51.dll");
    if (!lua) return;
    optical_gui_tried = true;
    optical_gui_mod = mod;
    const HfApi* api = HF_API(mod);
    if (!optical_gui_read_source(mod)) {
        api->log(mod, "Field Support: optical_view.lua, rocket_view.lua or rocket_terminal.lua missing or unreadable");
        return;
    }
    const uintptr_t exe = api->module_base("helldivers2.exe", NULL);
    if (exe && api->code_matches(exe + 0x9D8C0u,
        "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B 35 1A 28 97 01 8B D9"))
        optical_lua_resolve = (void* (*)(uint32_t))(exe + 0x9D8C0u);
    OpticalLuaCall pcall = (OpticalLuaCall)GetProcAddress(lua, "lua_pcall");
#define OPTICAL_LUA(field, name) *(FARPROC*)&optical_lua.field = GetProcAddress(lua, name)
    OPTICAL_LUA(top, "lua_gettop");
    OPTICAL_LUA(restore, "lua_settop");
    OPTICAL_LUA(get, "lua_getfield");
    OPTICAL_LUA(set, "lua_setfield");
    OPTICAL_LUA(type, "lua_type");
    OPTICAL_LUA(push, "lua_pushnumber");
    OPTICAL_LUA(closure, "lua_pushcclosure");
    OPTICAL_LUA(boolean, "lua_pushboolean");
    OPTICAL_LUA(userdata, "lua_touserdata");
    OPTICAL_LUA(load, "luaL_loadbuffer");
    OPTICAL_LUA(string, "lua_tolstring");
    OPTICAL_LUA(number, "lua_tonumber");
    OPTICAL_LUA(pointer, "lua_pushlightuserdata");
    OPTICAL_LUA(nil, "lua_pushnil");
#undef OPTICAL_LUA
    if (pcall && optical_lua.top && optical_lua.restore && optical_lua.get && optical_lua.set &&
        optical_lua.type && optical_lua.push && optical_lua.closure && optical_lua.boolean &&
        optical_lua.userdata && optical_lua.load && optical_lua.string && optical_lua.number && optical_lua.pointer && optical_lua.nil && api->hook_ex &&
        !api->hook_ex(mod, (uintptr_t)pcall, (void*)optical_gui_call, (void**)&optical_lua_next, 0))
        api->log(mod, "Optical view: waiting for engine Lua; persistent overlay HUD, cached character meshes");
    else api->log(mod, "Optical view: Lua interface unavailable");
}
