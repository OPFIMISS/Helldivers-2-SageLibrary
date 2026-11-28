#pragma once

/* Build 25480438: 6160EF..616124 transfers the acquired entity handle and
 * targeting-node index into the missile spawn packet. Keep that handoff. */
static uintptr_t rocket_native_instance(uint32_t weapon)
{
    const HfApi* api=HF_API(rocket_mod);
    const uintptr_t game=api->module_base("game.dll",NULL);
    const uintptr_t manager=optical_ptr(api,game+0x3326BD8);
    uint32_t index;
    if (!rocket_map(manager,0x840,weapon,&index)) return 0;
    const uintptr_t instances=optical_ptr(api,manager+0x870);
    return instances ? instances+(uintptr_t)index*20 : 0;
}

static bool rocket_native_point(const HfApi* api,uint32_t entity,uint32_t node,float point[3])
{
    if (!rocket_target_point || node>16) return false;
    const uintptr_t game=api->module_base("game.dll",NULL);
    const uintptr_t entities=(uintptr_t)hf_global_EntityManager(rocket_mod);
    uint32_t index;
    if (!optical_index(api,entities+0xF1AEB0,entity,&index) || index>=1048576) return false;
    const uintptr_t descriptors=entities+0xF32F18;
    const uintptr_t descriptor=descriptors+(uintptr_t)index*24;
    const uint32_t unit=optical_entity_unit(api,entities,entity,NULL);
    float root[3];
    if (!descriptors || !optical_unit_position(api,unit,root)) return false;
    if (node<16) {
        const uintptr_t factions=optical_ptr(api,game+0x3326CB8);
        uint32_t target_index,bone=UINT32_MAX,mask=1;
        if (!rocket_map(factions,0x12050,entity,&target_index)) return false;
        const uintptr_t records=optical_ptr(api,factions+0x12070);
        const uintptr_t masks=optical_ptr(api,factions+0x12078);
        if (!masks || api->read(masks+(uintptr_t)target_index*4,&mask,4) || (mask&1)) return false;
        if (!records || api->read(records+(uintptr_t)target_index*96+0x10+node*4,&bone,4) ||
            bone==UINT32_MAX || bone>65535) return false;
    }
    rocket_target_point((void*)optical_ptr(api,game+0x3326BD8),point,(const void*)descriptor,node);
    return isfinite(point[0]) && isfinite(point[1]) && isfinite(point[2]) &&
        rocket_distance2(root,point)<250.0f*250.0f;
}

static bool rocket_package_resident(HfMod* mod,uint64_t package)
{
    return HF_API(mod)->load_package(mod,package)==HF_PACKAGE_RESIDENT;
}

static bool rocket_native_node(const HfApi* api,uint32_t entity,uint32_t* node,float point[3])
{
    const uintptr_t game=api->module_base("game.dll",NULL);
    const uintptr_t factions=optical_ptr(api,game+0x3326CB8);
    uint32_t index,mask=1,bones[16];
    if (!rocket_map(factions,0x12050,entity,&index)) return false;
    const uintptr_t records=optical_ptr(api,factions+0x12070);
    const uintptr_t masks=optical_ptr(api,factions+0x12078);
    if (!records || !masks || api->read(masks+(uintptr_t)index*4,&mask,4) || (mask&1) ||
        api->read(records+(uintptr_t)index*96+0x10,bones,sizeof(bones))) return false;
    for (uint32_t n=0;n<16;++n) if (bones[n]!=UINT32_MAX && bones[n]<=65535) {
        if (rocket_native_point(api,entity,n,point)) {*node=n;return true;}
    }
    return false;
}

static bool rocket_native_lock(uint32_t weapon,uint32_t* entity,uint32_t* node,float point[3])
{
    const HfApi* api=HF_API(rocket_mod);
    const uintptr_t instance=rocket_native_instance(weapon);
    uint32_t handle=0x7FFF;
    *entity=0;*node=16;
    if (!instance || !rocket_handle_entity || api->read(instance+0xC,&handle,4) ||
        handle==0x7FFF || api->read(instance+0x10,node,4)) return false;
    rocket_handle_entity(entity,handle);
    if (!*entity || *entity==UINT32_MAX || !rocket_native_point(api,*entity,*node,point)) return false;
    return true;
}

static void rocket_native_spawn(HfMod* mod,const HfSpawnEvent* event,void* user)
{
    (void)mod;(void)user;
    if (!event->authority) return;
    unsigned variant=5;
    for (unsigned i=0;i<5;++i) if (event->resource==rocket_missiles[i]) {variant=i;break;}
    if (variant!=3 && variant!=4) return;
    for (unsigned i=0;i<256;++i) if (!rocket_native_flights[i].entity) {
        memset(&rocket_native_flights[i],0,sizeof(rocket_native_flights[i]));
        rocket_native_flights[i].entity=event->entity;
        rocket_native_flights[i].variant=variant;
        return;
    }
}
