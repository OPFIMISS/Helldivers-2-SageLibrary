#pragma once

#include "rocket_blast.h"
#include "rocket_targets.h"
#include "rocket_audio.h"

#define ROCKET_PREFIX "liuli/field_support/rockets/"
#define ROCKET_RACK 0xB7791DA91F13488Cull
#define ROCKET_POD 0x73F8498BFFDCF415ull
#define ROCKET_COMMANDO_PACKAGE 0xC0052E5B38E18C33ull
#define ROCKET_SPEAR_PACKAGE 0x530BB611A14B6CE3ull
#define ROCKET_AIRBURST_PACKAGE 0xF7646E79610C124Dull
#define ROCKET_SPEAR_MISSILE 0x1A4323835B84CEA7ull

enum { ROCKET_SINGLE, ROCKET_SALVO, ROCKET_FOCUS, ROCKET_AIRBURST };
static HfMod* rocket_mod;
static HfRef rocket_weapons[2], rocket_pack;
static HfRef rocket_missiles[5];
static uint32_t rocket_projectiles[5], rocket_damage[4], rocket_contact_damage;
static uint32_t rocket_weapon, rocket_avatar, rocket_kind;
static uint32_t rocket_lock, rocket_lock_candidate;
static uint32_t rocket_function;
static bool rocket_guidance=true, rocket_ready, rocket_native_ready, rocket_assets, rocket_hooks;
static bool rocket_airburst_assets;
static float rocket_airburst_clock;
static float rocket_lock_time, rocket_scan_clock, rocket_reload_time, rocket_package_clock;
static int rocket_guide_key=VK_F6, rocket_function_key=VK_F7, rocket_reload_key='R';
static bool rocket_previous_guide, rocket_previous_function, rocket_previous_reload;
static uint32_t rocket_pack_rounds, rocket_mag_rounds;
static float rocket_cooldowns[2]={120.0f,480.0f};
static _Atomic uint32_t rocket_ui_state;
static _Atomic uint32_t rocket_ui_count;
static _Atomic uint32_t rocket_ui_target_unit;
static _Atomic uint32_t rocket_ui_target_entity;
static _Atomic uint32_t rocket_ui_keys;
static _Atomic float rocket_ui_distance;
static _Atomic float rocket_ui_point[3], rocket_ui_progress;
static _Atomic float rocket_camera[6];
static _Atomic ULONGLONG rocket_camera_at;
static _Atomic uint32_t rocket_weak_entity;
static _Atomic float rocket_weak_position[3];
static _Atomic ULONGLONG rocket_weak_at;

typedef uint32_t (*RocketRealize)(void*, const uint8_t*, const uint8_t*);
typedef void (*RocketPellet)(void*,uint32_t,uint32_t,const uint8_t*,uint32_t,float,uint32_t,uint8_t,float,float,float,uint8_t);
typedef uint32_t (*RocketCount)(void*,uint32_t);
typedef void (*RocketConsume)(void*,uint32_t);
typedef void (*RocketSetMagazine)(void*,uint32_t,uint32_t);
typedef bool (*RocketCanFire)(void*,uint32_t);
typedef void (*RocketReplicate)(uint32_t,uint32_t,const void*);
static RocketRealize rocket_realize_next;
static RocketPellet rocket_pellet_next;
static RocketCount rocket_count_next;
static RocketConsume rocket_consume_next;
static RocketSetMagazine rocket_set_magazine;
static RocketCanFire rocket_can_fire_next;
static RocketReplicate rocket_replicate;
static uintptr_t (*rocket_live_settings)(const void*);
static uint32_t* (*rocket_handle_entity)(uint32_t*,uint32_t);
static uint32_t (*rocket_entity_handle)(uint32_t);
static float* (*rocket_target_point)(void*,float*,const void*,uint32_t);
static _Thread_local unsigned rocket_pending_cost=1;
static _Thread_local uint32_t rocket_pending_weapon;
static _Thread_local struct {
    bool active, homing, split, retarget;
    uint32_t target, seed, source, type;
    float fuse;
} rocket_shot;

typedef struct RocketFlight {
    bool active, homing, split, retarget;
    uint32_t type, target, seed, source;
    float age, distance, fuse;
} RocketFlight;
static RocketFlight rocket_flights[2048];
static struct { uint32_t entity; unsigned variant; float age,distance,last[3]; bool positioned; }
    rocket_native_flights[256];
static struct {
    RocketFlight flight;
    uint32_t type;
    ULONGLONG expires;
} rocket_intents[128];
static unsigned rocket_intent_cursor;
static SRWLOCK rocket_intent_lock=SRWLOCK_INIT;

static void rocket_queue_shot(void)
{
    AcquireSRWLockExclusive(&rocket_intent_lock);
    const unsigned at=rocket_intent_cursor++%128;
    rocket_intents[at].flight=(RocketFlight){.active=true,.homing=rocket_shot.homing,
        .split=rocket_shot.split,.retarget=rocket_shot.retarget,.target=rocket_shot.target,
        .seed=rocket_shot.seed,.source=rocket_shot.source,.type=rocket_shot.type,.fuse=rocket_shot.fuse};
    rocket_intents[at].type=rocket_shot.type;
    rocket_intents[at].expires=GetTickCount64()+3000;
    ReleaseSRWLockExclusive(&rocket_intent_lock);
}
typedef struct RocketPeer {
    uint64_t peer;
    uint32_t weapon,target,mode;
    bool guided;
    float lease;
} RocketPeer;
static RocketPeer rocket_peers[4];
typedef struct RocketMessage {
    uint32_t magic,weapon,target,mode,guided;
} RocketMessage;
static RocketMessage rocket_last_message;
static float rocket_heartbeat;

static bool rocket_map(uintptr_t manager, unsigned offset, uint32_t id, uint32_t* index)
{
    return manager && id && id!=UINT32_MAX && optical_index(HF_API(rocket_mod),manager+offset,id,index) && *index<65536;
}

#include "rocket_native.h"

static uint32_t rocket_network_id(uint32_t entity)
{
    const HfApi* api=HF_API(rocket_mod);
    const uintptr_t entities=(uintptr_t)hf_global_EntityManager(rocket_mod);
    uint32_t i,network=UINT32_MAX;
    if (entities && optical_index(api,entities+0xF1AEB0,entity,&i) && i<1048576)
        api->read(entities+0xF32F18+(uintptr_t)i*24+0x10,&network,4);
    return network;
}

static void rocket_message(HfMod* mod,const HfEventInfo* info,void* user)
{
    (void)user;
    const HfMessageEvent* event=(const HfMessageEvent*)info;
    if (event->bytes!=sizeof(RocketMessage)) return;
    RocketMessage packet;
    memcpy(&packet,event->data,sizeof(packet));
    if (packet.magic!=0x31544B52u || packet.mode>ROCKET_AIRBURST || packet.guided>1) return;
    const HfApi* api=HF_API(mod);
    if (api->game_state(mod)!=HF_STATE_MISSION) return;
    unsigned at=4;
    for (unsigned i=0;i<4;++i) if (rocket_peers[i].peer==event->from) {at=i;break;}
    if (packet.weapon==UINT32_MAX) {if (at<4) memset(&rocket_peers[at],0,sizeof(rocket_peers[at]));return;}
    const uintptr_t game=api->module_base("game.dll",NULL);
    const uint32_t avatar=optical_peer_avatar(mod,game,event->from);
    const uint32_t weapon=api->entity_of_network(mod,packet.weapon);
    const uintptr_t inventory=(uintptr_t)hf_global_InventoryManager(mod);
    uint32_t index,support=0;
    HfRef resource=0;
    if (!avatar || !rocket_map(inventory,0x28,avatar,&index)) return;
    const uintptr_t records=optical_ptr(api,inventory+0x50);
    if (!records || api->read(records+index*0x30+8,&support,4) || support!=weapon ||
        !optical_entity_unit(api,(uintptr_t)hf_global_EntityManager(mod),weapon,&resource) ||
        (resource!=rocket_weapons[0] && resource!=rocket_weapons[1])) return;
    uint32_t target=packet.target==UINT32_MAX ? 0 : api->entity_of_network(mod,packet.target);
    if (target && target!=UINT32_MAX) {
        HfRef body=0;
        const uint32_t target_unit=optical_entity_unit(api,(uintptr_t)hf_global_EntityManager(mod),target,&body);
        const uint32_t avatar_unit=optical_entity_unit(api,(uintptr_t)hf_global_EntityManager(mod),avatar,NULL);
        float from[3],to[3];
        if (!target_unit || !optical_unit_position(api,avatar_unit,from) ||
            !optical_unit_position(api,target_unit,to) || rocket_distance2(from,to)>610.0f*610.0f) target=0;
    } else target=0;
    if (at==4) for (unsigned i=0;i<4;++i) if (!rocket_peers[i].peer) {at=i;break;}
    if (at<4) rocket_peers[at]=(RocketPeer){event->from,weapon,target,packet.mode,packet.guided!=0,2.0f};
}

static void rocket_network_tick(HfMod* mod,float dt)
{
    for (unsigned i=0;i<4;++i) if (rocket_peers[i].peer && (rocket_peers[i].lease-=dt)<=0)
        memset(&rocket_peers[i],0,sizeof(rocket_peers[i]));
    HfSession session={0};
    const HfApi* api=HF_API(mod);
    api->session(mod,&session);
    if (session.role==HF_SESSION_SOLO) return;
    rocket_heartbeat-=dt;
    const RocketMessage packet={0x31544B52u,rocket_kind?rocket_network_id(rocket_weapon):UINT32_MAX,
        rocket_lock?rocket_network_id(rocket_lock):UINT32_MAX,rocket_function,rocket_kind==1 || rocket_guidance};
    if (memcmp(&packet,&rocket_last_message,sizeof(packet)) || rocket_heartbeat<=0) {
        api->send(mod,HF_TO_ALL,&packet,sizeof(packet));
        rocket_last_message=packet;
        rocket_heartbeat=0.5f;
    }
}

static uint32_t rocket_pack_entity(uint32_t avatar)
{
    const HfApi* api=HF_API(rocket_mod);
    const uintptr_t inventory=(uintptr_t)hf_global_InventoryManager(rocket_mod);
    uint32_t i,pack=0;
    if (!rocket_map(inventory,0x28,avatar,&i)) return 0;
    const uintptr_t records=optical_ptr(api,inventory+0x50);
    HfRef resource=0;
    if (!records || api->read(records+i*0x30+0x0C,&pack,4) ||
        !optical_entity_unit(api,(uintptr_t)hf_global_EntityManager(rocket_mod),pack,&resource)) return 0;
    return resource==rocket_pack ? pack : 0;
}

static bool rocket_ammunition(uint32_t weapon, uint32_t* magazine, uint32_t* pack_id, uint32_t* reserve)
{
    const HfApi* api=HF_API(rocket_mod);
    const uintptr_t game=api->module_base("game.dll",NULL);
    const uintptr_t mags=optical_ptr(api,game+0x3326648);
    uint32_t i;
    if (!rocket_map(mags,0x20,weapon,&i)) return false;
    const uintptr_t records=optical_ptr(api,mags+0x48);
    if (!records || api->read(records+i*16,magazine,4)) return false;
    /* Keep one physical safety round for manual backpack reloads. */
    if (weapon==rocket_weapon && rocket_kind==2) {
        if (!*magazine || *magazine>5) return false;
        --*magazine;
    } else if (*magazine>4) return false;
    *reserve=0;
    *pack_id=rocket_pack_entity(rocket_avatar);
    if (!*pack_id) return true;
    const uintptr_t deposits=optical_ptr(api,game+0x33265E8);
    if (!rocket_map(deposits,0x20,*pack_id,&i)) return false;
    const uintptr_t counts=optical_ptr(api,deposits+0x50);
    return counts && !api->read(counts+i*8,reserve,4) && *reserve<=24;
}

static bool rocket_set_reserve(uint32_t pack,uint32_t expected,uint32_t amount)
{
    const HfApi* api=HF_API(rocket_mod);
    const uintptr_t game=api->module_base("game.dll",NULL);
    const uintptr_t manager=optical_ptr(api,game+0x33265E8);
    uint32_t i,current=0,network=0;
    if (!rocket_replicate || amount>24 || !rocket_map(manager,0x20,pack,&i)) return false;
    const uintptr_t records=optical_ptr(api,manager+0x50);
    const uintptr_t descriptors=optical_ptr(api,manager+0x38);
    const uintptr_t descriptor=descriptors ? optical_ptr(api,descriptors+i*8) : 0;
    const uintptr_t count=records+i*8;
    if (!records || !descriptor || api->read(count,&current,4) || current!=expected ||
        api->read(descriptor+0x10,&network,4) || api->write(count,&amount,4)) return false;
    /* 87FE30..87FE5A: Deposit's native count and replicated property id.
     * Pass the live field, exactly as the original refill routine does. */
    rocket_replicate(network,0x69889752u,(const void*)count);
    return true;
}

static bool rocket_set_loaded(uint32_t weapon,uint32_t expected,uint32_t amount)
{
    const HfApi* api=HF_API(rocket_mod);
    const uintptr_t game=api->module_base("game.dll",NULL);
    const uintptr_t manager=optical_ptr(api,game+0x3326648);
    uint32_t i,current;
    if (!rocket_set_magazine || amount>4 || !rocket_map(manager,0x20,weapon,&i)) return false;
    const uintptr_t records=optical_ptr(api,manager+0x48);
    const uint32_t physical_expected=expected+1, physical_amount=amount+1;
    if (!records || api->read(records+i*16,&current,4) || current!=physical_expected ||
        api->write(records+i*16,&physical_amount,4)) return false;
    /* 74481C and 74487E: change the live magazine first, then publish +50's
     * replicated count through 76E2A0. The network setter alone cannot reload. */
    rocket_set_magazine(NULL,weapon,physical_amount);
    return true;
}

static bool rocket_allowed(uint32_t weapon)
{
    const HfApi* api=HF_API(rocket_mod);
    if (!rocket_ready || !rocket_native_ready || !rocket_hooks || !rocket_assets || api->game_state(rocket_mod)!=HF_STATE_MISSION) return false;
    if (weapon!=rocket_weapon || !rocket_kind) return true; /* remote shots use their native RPC */
    const unsigned cost=rocket_kind==2 && rocket_function==ROCKET_SALVO ? 4 :
        rocket_kind==2 && rocket_function==ROCKET_FOCUS ? 2 : 1;
    uint32_t mag,pack,reserve;
    if (!rocket_ammunition(weapon,&mag,&pack,&reserve) || mag<cost) return false;
    const bool needs_lock=rocket_kind==1 || (rocket_guidance && rocket_function!=ROCKET_AIRBURST);
    if (needs_lock && (!rocket_lock || rocket_lock_time<0.35f)) return false;
    if (rocket_kind==2 && rocket_function==ROCKET_AIRBURST && !rocket_airburst_assets) return false;
    return rocket_kind!=2 || !rocket_reload_time;
}

static uint32_t rocket_round_count(void* manager, uint32_t entity)
{
    const uint32_t count=rocket_count_next(manager,entity);
    /* Controller queries must not put the reloadable variant into an empty
     * transition. Firing and the custom HUD read the actual magazine. */
    if (!count) {
        const HfApi* api=HF_API(rocket_mod);
        HfRef resource=0;
        if (optical_entity_unit(api,(uintptr_t)hf_global_EntityManager(rocket_mod),entity,&resource) &&
            resource==rocket_weapons[1]) return 1;
    }
    return count;
}

static bool rocket_can_fire(void* manager,uint32_t entity)
{
    if (entity==rocket_weapon && rocket_kind && !rocket_allowed(entity)) return false;
    if (entity==rocket_weapon && rocket_kind==2 &&
        (!rocket_guidance || rocket_function==ROCKET_AIRBURST)) return true;
    return rocket_can_fire_next(manager,entity);
}

static void rocket_consume(void* manager, uint32_t entity)
{
    unsigned cost=entity==rocket_pending_weapon ? rocket_pending_cost : 1;
    rocket_pending_cost=1;
    rocket_pending_weapon=0;
    for (unsigned i=0;i<cost;++i) rocket_consume_next(manager,entity);
}

static uint32_t rocket_realize(void* system, const uint8_t* request, const uint8_t* auxiliary)
{
    const HfApi* api=HF_API(rocket_mod);
    uint32_t source=0,type=0,seed=0;
    uintptr_t settings=0;
    if (request && !api->read((uintptr_t)request+0x10,&settings,8) && settings) {
        api->read(settings,&type,4);
        api->read((uintptr_t)request+0x18,&source,4);
        if (auxiliary) api->read((uintptr_t)auxiliary+4,&seed,4);
    }
    const uint32_t result=rocket_realize_next(system,request,auxiliary);
    if (result<2048) {
        rocket_flights[result].active=false;
        if (rocket_shot.active && rocket_shot.source==source && rocket_shot.type==type) {
            rocket_flights[result]=(RocketFlight){
                .active=true,.homing=rocket_shot.homing,.split=rocket_shot.split,
                .retarget=rocket_shot.retarget,
                .target=rocket_shot.target,.seed=rocket_shot.seed,.source=rocket_shot.source,.type=type,
                .fuse=rocket_shot.fuse
            };
        }
        AcquireSRWLockExclusive(&rocket_intent_lock);
        const ULONGLONG now=GetTickCount64();
        unsigned match=128;
        for (unsigned i=0;i<128;++i)
            if (rocket_intents[i].expires>=now && rocket_intents[i].flight.source==source &&
                rocket_intents[i].type==type) {
                if (rocket_intents[i].flight.seed==seed) {match=i;break;}
                /* Some native queue paths derive a new per-projectile seed.
                 * Fall back only within this private type and firing weapon. */
                if (match==128 || rocket_intents[i].expires<rocket_intents[match].expires) match=i;
            }
        if (match<128) {
            rocket_flights[result]=rocket_intents[match].flight;
            rocket_intents[match].expires=0;
        }
        ReleaseSRWLockExclusive(&rocket_intent_lock);
    }
    return result;
}

static void rocket_fire(void* manager,uint32_t index,uint32_t type,const uint8_t* transform,
    uint32_t owner,float a6,uint32_t seed,uint8_t a8,float a9,float a10,float a11,uint8_t a12)
{
    bool custom=rocket_kind || type==rocket_projectiles[0] || type==rocket_projectiles[1];
    for (unsigned i=0;!custom && i<4;++i) custom=rocket_peers[i].peer!=0;
    if (!custom) {
        rocket_pellet_next(manager,index,type,transform,owner,a6,seed,a8,a9,a10,a11,a12);
        return;
    }
    const HfApi* api=HF_API(rocket_mod);
    const uintptr_t descriptors=manager ? optical_ptr(api,(uintptr_t)manager+0x68) : 0;
    const uintptr_t descriptor=descriptors ? optical_ptr(api,descriptors+index*8) : 0;
    uint32_t source=0;
    HfRef resource=0;
    if (descriptor) api->read(descriptor+8,&source,4);
    if (source) optical_entity_unit(api,(uintptr_t)hf_global_EntityManager(rocket_mod),source,&resource);
    int which=-1;
    for (unsigned i=0;i<2;++i) if (resource==rocket_weapons[i]) which=(int)i;
    if (which<0) {
        rocket_pellet_next(manager,index,type,transform,owner,a6,seed,a8,a9,a10,a11,a12);
        return;
    }
    rocket_pending_cost=0;
    rocket_pending_weapon=source;
    if (!transform || !owner || !rocket_ready || !rocket_native_ready || !rocket_hooks || !rocket_assets) return;
    if (!source) return;
    type=rocket_projectiles[which];
    const bool local=source==rocket_weapon && rocket_kind==(uint32_t)which+1;
    rocket_pending_weapon=source;
    if (local && !rocket_allowed(source)) return;
    RocketPeer* remote=NULL;
    if (!local) for (unsigned i=0;i<4;++i)
        if (rocket_peers[i].weapon==source && rocket_peers[i].lease>0) {remote=&rocket_peers[i];break;}
    const unsigned mode=which==1 ? (local?rocket_function:remote?remote->mode:ROCKET_SINGLE) : ROCKET_SINGLE;
    if (mode==ROCKET_AIRBURST && !rocket_airburst_assets) return;
    bool homing=which==0 || (local?rocket_guidance:remote?remote->guided:true);
    uint32_t selected=local?rocket_lock:remote?remote->target:0;
    if (!local && homing) {
        uint32_t native_target=0,native_node=16;
        float native_point[3];
        if (rocket_native_lock(source,&native_target,&native_node,native_point)) selected=native_target;
    }
    if (!selected && homing && !local) {
        const float* basis=(const float*)transform;
        RocketTarget* fallback=rocket_acquire(basis+12,basis+4);
        if (fallback) selected=fallback->entity;
    }
    /* Airburst must also launch at small enemies that Spear cannot acquire. */
    if (mode==ROCKET_AIRBURST && !selected) homing=false;
    if (homing && !selected) return;
    /* Spear's entity branch serializes the target and node into each missile.
     * Only direct fire uses the primitive straight-flight branch. */
    const uintptr_t settings=rocket_live_settings ? rocket_live_settings((const void*)descriptor) : 0;
    uint64_t old_entity=0,old_function_entity=0;
    const bool native=homing || mode==ROCKET_SALVO;
    const unsigned variant=mode==ROCKET_SALVO?4:mode==ROCKET_FOCUS?2:
        mode==ROCKET_AIRBURST?3:(unsigned)which;
    const uint64_t missile=native ? rocket_missiles[variant] : 0;
    const uintptr_t lock_instance=rocket_native_instance(source);
    uint32_t previous_target[2]={0x7FFF,16};
    if (native && (!lock_instance || api->read(lock_instance+0xC,previous_target,8))) return;
    if (!settings || api->read(settings+0x28,&old_entity,8) ||
        api->read(settings+0x248,&old_function_entity,8)) return;
    if (api->write(settings+0x28,&missile,8)) return;
    if (api->write(settings+0x248,&missile,8)) {
        api->write(settings+0x28,&old_entity,8);return;
    }
    const unsigned count=mode==ROCKET_SALVO ? 4 : 1;
    if (mode==ROCKET_AIRBURST)
        api->log(rocket_mod,"Rockets: airburst launched, %s carrier, target %u",
            native?"native seeking":"direct",selected);
    uint32_t assigned[4]={0};
    rocket_pending_cost=mode==ROCKET_SALVO ? 4 : mode==ROCKET_FOCUS ? 2 : 1;
    _Alignas(16) float shot[16];
    for (unsigned n=0;n<count;++n) {
        memcpy(shot,transform,sizeof(shot));
        for (unsigned j=0;j<3;++j)
            shot[12+j]+=shot[4+j]*0.8f+shot[j]*((float)n-1.5f)*0.035f;
        const uint32_t round=mode==ROCKET_FOCUS ? rocket_projectiles[2] :
            mode==ROCKET_AIRBURST ? rocket_projectiles[3] : type;
        rocket_shot.active=true;
        rocket_shot.homing=false;
        rocket_shot.split=mode==ROCKET_AIRBURST;
        rocket_shot.retarget=mode==ROCKET_SALVO;
        rocket_shot.target=selected;
        if (mode==ROCKET_SALVO) {
            RocketTarget* individual=rocket_salvo_target(shot+12,shot+4,selected,assigned,n);
            rocket_shot.target=individual ? individual->entity : 0;
            assigned[n]=rocket_shot.target;
        }
        if (native) {
            uint32_t target=rocket_shot.target,node=16;
            float point[3];
            uint32_t native_entity=0,native_node=16;
            if (rocket_native_lock(source,&native_entity,&native_node,point) && native_entity==target)
                node=native_node;
            else if (target && !rocket_native_node(api,target,&node,point)) node=16;
            const uint32_t acquired[2]={target?rocket_entity_handle(target):0x7FFF,node};
            if (api->write(lock_instance+0xC,acquired,8)) break;
        }
        rocket_shot.seed=seed+n*0x9E3779B9u;
        rocket_shot.source=source;
        rocket_shot.type=round;
        rocket_shot.fuse=0;
        /* The native projectile queue may realize this after 615940 returns.
         * Match source/type/seed instead of depending on a temporary TLS scope. */
        if (!native) rocket_queue_shot();
        rocket_pellet_next(manager,index,round,(const uint8_t*)shot,owner,a6,
            rocket_shot.seed,a8,a9,a10,a11,n==0?a12:0);
        rocket_shot.active=false;
    }
    if (native) api->write(lock_instance+0xC,previous_target,8);
    api->write(settings+0x248,&old_function_entity,8);
    api->write(settings+0x28,&old_entity,8);
}

static void rocket_split(void* system,unsigned slot,const float position[3],const float velocity[3],const RocketFlight* parent,bool native)
{
    float forward[3]={velocity[0],velocity[1],velocity[2]};
    if (!rocket_normalize(forward)) return;
    RocketTarget* threat=rocket_find_target(parent->target);
    float nearest=1e30f;
    if (!threat) for (unsigned i=0;i<rocket_target_count;++i) {
        const float d=rocket_distance2(position,rocket_targets[i].position);
        if (d<nearest) { nearest=d;threat=&rocket_targets[i]; }
    }
    float centre[3];
    for (unsigned j=0;j<3;++j) centre[j]=threat ? threat->position[j] : position[j]+forward[j]*10;
    centre[2]-=threat ? 0.8f : 2.0f;
    const HfApi* api=HF_API(rocket_mod);
    HfSession session={.role=HF_SESSION_JOINER};
    api->session(rocket_mod,&session);
    HfFn_FireProjectile fire=hf_fn_FireProjectile(rocket_mod);
    if (!fire && session.role!=HF_SESSION_JOINER) return;
    /* Clear the carrier through the game's normal pool retirement, then create
     * six collision-tested rounds. Never detonate a live carrier at the muzzle. */
    typedef void (*Retire)(void*,uint32_t);
    const uintptr_t game=api->module_base("game.dll",NULL);
    if (native) {
        typedef void (*Remove)(void*,uint32_t);
        ((Remove)(game+0xFDC310))(hf_global_EntityManager(rocket_mod),slot);
    } else ((Retire)(game+0x13A8700))(system,slot);
    /* Retire each local carrier, but only the host creates damaging children.
     * This follows FireProjectile's SDK16 authority contract. */
    if (session.role==HF_SESSION_JOINER) return;
    for (unsigned n=0;n<6;++n) {
        const float angle=n*1.047197551f+(parent->seed&255)*0.02454369f;
        float direction[3],from[3];
        for (unsigned j=0;j<3;++j) {
            from[j]=position[j]+forward[j]*0.4f;
            const float spread=j==0 ? cosf(angle)*3.2f : j==1 ? sinf(angle)*3.2f : 0;
            direction[j]=centre[j]+spread-from[j];
        }
        const float path=sqrtf(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
        rocket_normalize(direction);
        rocket_shot.active=true;
        rocket_shot.homing=rocket_shot.split=rocket_shot.retarget=false;
        rocket_shot.source=parent->source;
        rocket_shot.target=0;
        rocket_shot.seed=parent->seed+n;
        rocket_shot.type=rocket_projectiles[4];
        rocket_shot.fuse=fminf(0.65f,fmaxf(0.04f,path/55.0f));
        rocket_queue_shot();
        fire(system,rocket_projectiles[4],from,direction,parent->source,0,0);
        rocket_shot.active=false;
    }
}

static void rocket_update_flights(HfMod* mod,float dt)
{
    const HfApi* api=HF_API(mod);
    const uintptr_t system=(uintptr_t)hf_global_ProjectileSystem(mod);
    if (!system) return;
    for (unsigned slot=0;slot<2048;++slot) {
        RocketFlight* f=&rocket_flights[slot];
        if (!f->active) continue;
        uint16_t flags=0;
        float position[3],velocity[3],distance=0;
        uint32_t type=0;
        const uintptr_t flight=system+12352+slot*112;
        if (api->read(system+8252+slot*2,&flags,2) || !(flags&2) ||
            api->read(flight,position,12) || api->read(flight+12,velocity,12) ||
            api->read(system+938048+slot*4,&type,4) || type!=f->type ||
            api->read(flight+48,&distance,4) || !isfinite(distance) || distance+0.1f<f->distance ||
            (f->age+=dt)>8.0f) { f->active=false;continue; }
        f->distance=distance;
        if (f->type==rocket_projectiles[4]) {
            if (f->fuse>0) {
                const float remaining=fmaxf(0.001f,f->fuse-distance/55.0f);
                api->write(flight+52,&remaining,4);
            }
            f->active=false;
            continue;
        }
        RocketTarget* target=rocket_find_target(f->target);
        if (!target && f->homing && f->retarget) {
            target=rocket_retarget(position,f->seed);
            if (target) f->target=target->entity;
        }
        if (f->split && distance>=20.0f && f->age>0.1f) {
            RocketTarget* nearby=rocket_near_threat(position,velocity,dt);
            if (nearby) {
                RocketFlight parent=*f;
                parent.target=nearby->entity;
                f->active=false;
                rocket_split((void*)system,slot,position,velocity,&parent,false);
                continue;
            }
        }
        if (!f->homing || !target) continue;
        float aim[3]={target->position[0],target->position[1],target->position[2]};
        const ULONGLONG now=GetTickCount64();
        if (atomic_load(&rocket_weak_entity)==target->entity && now-atomic_load(&rocket_weak_at)<250)
            for (unsigned j=0;j<3;++j) aim[j]=atomic_load(&rocket_weak_position[j]);
        if (f->split) aim[2]+=4.5f;
        float direction[3];
        for (unsigned j=0;j<3;++j) direction[j]=aim[j]-position[j];
        if (!rocket_normalize(direction)) continue;
        float current[3]={velocity[0],velocity[1],velocity[2]};
        const float speed=sqrtf(velocity[0]*velocity[0]+velocity[1]*velocity[1]+velocity[2]*velocity[2]);
        if (speed<1 || !rocket_normalize(current)) continue;
        const float blend=fminf(1.0f,dt*8.0f);
        for (unsigned j=0;j<3;++j) direction[j]=current[j]*(1-blend)+direction[j]*blend;
        if (!rocket_normalize(direction)) continue;
        for (unsigned j=0;j<3;++j) velocity[j]=direction[j]*speed;
        api->write(flight+12,velocity,12);
    }
}

static void rocket_update_native(HfMod* mod,float dt)
{
    const HfApi* api=HF_API(mod);
    const uintptr_t game=api->module_base("game.dll",NULL);
    const uintptr_t manager=optical_ptr(api,game+0x3326DF0);
    const uintptr_t records=optical_ptr(api,manager+0x48);
    const uintptr_t instances=optical_ptr(api,manager+0x50);
    if (!manager || !records || !instances) return;
    for (unsigned i=0;i<256;++i) {
        uint32_t entity=rocket_native_flights[i].entity,index;
        if (!entity) continue;
        if (!rocket_map(manager,0x20,entity,&index)) {rocket_native_flights[i].entity=0;continue;}
        const uintptr_t record=records+(uintptr_t)index*0xC0;
        const uintptr_t instance=instances+(uintptr_t)index*0x38;
        float position[3],velocity[3]={0,1,0};
        if (api->read(record+0x48,position,12)) continue;
        rocket_native_flights[i].age+=dt;
        if (rocket_native_flights[i].positioned) {
            for (unsigned j=0;j<3;++j) velocity[j]=(position[j]-rocket_native_flights[i].last[j])/fmaxf(dt,0.001f);
            rocket_native_flights[i].distance+=sqrtf(rocket_distance2(position,rocket_native_flights[i].last));
        }
        memcpy(rocket_native_flights[i].last,position,12);
        rocket_native_flights[i].positioned=true;
        if (rocket_native_flights[i].variant==3 && rocket_native_flights[i].distance>=20 &&
            rocket_native_flights[i].age>0.1f) {
            RocketTarget* nearby=rocket_near_threat(position,velocity,dt);
            if (nearby) {
                uint32_t source=0;
                api->read(record+0x84,&source,4);
                const RocketFlight parent={.target=nearby->entity,.source=source,.seed=entity};
                rocket_native_flights[i].entity=0;
                rocket_split(hf_global_ProjectileSystem(mod),entity,position,velocity,&parent,true);
            }
        } else if (rocket_native_flights[i].variant==4) {
            uint32_t handle=0x7FFF,target=0;
            if (api->read(instance,&handle,4)) continue;
            rocket_handle_entity(&target,handle);
            if (target && target!=UINT32_MAX && rocket_find_target(target)) continue;
            RocketTarget* next=rocket_retarget(position,entity+i);
            if (!next) continue;
            const uint32_t acquired[2]={rocket_entity_handle(next->entity),next->node};
            if (!api->write(instance,acquired,8)) {
                const uint32_t network=rocket_network_id(entity);
                if (network!=UINT32_MAX) rocket_replicate(network,0xAC08FE53u,(const void*)instance);
            }
        }
    }
}

static bool rocket_pressed(int key)
{
    DWORD process=0;
    GetWindowThreadProcessId(GetForegroundWindow(),&process);
    return process==GetCurrentProcessId() && (GetAsyncKeyState(key)&0x8000)!=0;
}

static void rocket_weapon_changed(HfMod* mod,const HfWeaponEvent* e,void* user)
{
    (void)mod;(void)user;
    if (rocket_reload_time>0) rocket_reload_audio_stop();
    rocket_weapon=e->entity;
    rocket_avatar=e->avatar;
    rocket_kind=e->resource==rocket_weapons[0] ? 1 : e->resource==rocket_weapons[1] ? 2 : 0;
    rocket_lock=rocket_lock_candidate=0;
    rocket_lock_time=rocket_reload_time=0;
}

static void rocket_tick(HfMod* mod,float dt,void* user)
{
    (void)user;
    const HfApi* api=HF_API(mod);
    const bool mission=api->game_state(mod)==HF_STATE_MISSION;
    if (!mission) {
        memset(rocket_flights,0,sizeof(rocket_flights));
        memset(rocket_native_flights,0,sizeof(rocket_native_flights));
        AcquireSRWLockExclusive(&rocket_intent_lock);
        memset(rocket_intents,0,sizeof(rocket_intents));
        ReleaseSRWLockExclusive(&rocket_intent_lock);
        memset(rocket_weak_cache,0,sizeof(rocket_weak_cache));
        atomic_store(&rocket_camera_at,0);
        atomic_store(&rocket_weak_at,0);
        rocket_target_count=rocket_kind=rocket_lock=0;
        rocket_assets=false;
        rocket_airburst_assets=false;
        rocket_airburst_clock=0;
        rocket_previous_guide=rocket_previous_function=rocket_previous_reload=false;
        atomic_store(&rocket_ui_target_unit,0);
        atomic_store(&rocket_ui_target_entity,0);
        if (rocket_reload_time>0) rocket_reload_audio_stop();
        rocket_reload_time=0;
        atomic_store(&rocket_ui_state,0);
        memset(rocket_peers,0,sizeof(rocket_peers));
        memset(&rocket_last_message,0,sizeof(rocket_last_message));
        rocket_heartbeat=0;
        return;
    }
    if (!rocket_assets && (rocket_package_clock-=dt)<=0) {
        rocket_package_clock=0.5f;
        rocket_assets=rocket_package_resident(mod,ROCKET_SPEAR_PACKAGE) &&
            api->load_package(mod,ROCKET_COMMANDO_PACKAGE)==HF_PACKAGE_RESIDENT &&
            api->load_package(mod,0xDE96CD7CC69628BCull)==HF_PACKAGE_RESIDENT &&
            api->load_package(mod,0xF24CFA612428175Bull)==HF_PACKAGE_RESIDENT;
    }
    if (!rocket_airburst_assets && (rocket_airburst_clock-=dt)<=0) {
        rocket_airburst_clock=0.5f;
        rocket_airburst_assets=rocket_package_resident(mod,ROCKET_AIRBURST_PACKAGE);
    }
    bool active_flight=false;
    for (unsigned i=0;i<2048;++i) if (rocket_flights[i].active) {active_flight=true;break;}
    bool native_flight=false;
    for (unsigned i=0;i<256;++i) if (rocket_native_flights[i].entity) {native_flight=true;break;}
    float origin[3],forward[3];
    const bool camera=GetTickCount64()-atomic_load(&rocket_camera_at)<300;
    for (unsigned j=0;j<3;++j) { origin[j]=atomic_load(&rocket_camera[j]);forward[j]=atomic_load(&rocket_camera[j+3]); }
    if (!camera && rocket_avatar) {
        const uint32_t unit=optical_entity_unit(api,(uintptr_t)hf_global_EntityManager(mod),rocket_avatar,NULL);
        optical_unit_position(api,unit,origin);
    }
    if ((rocket_kind || active_flight || native_flight) && (rocket_scan_clock-=dt)<=0) {
        rocket_scan_clock=0.1f;
        rocket_scan_targets(mod,origin);
    }
    const uint32_t weak=atomic_load(&rocket_weak_entity);
    if (weak && GetTickCount64()-atomic_load(&rocket_weak_at)<250) {
        RocketTarget* t=rocket_find_target(weak);
        float root[3],point[3];
        if (t && optical_unit_position(api,t->unit,root)) {
            for (unsigned j=0;j<3;++j) point[j]=atomic_load(&rocket_weak_position[j]);
            if (rocket_distance2(root,point)<35.0f*35.0f) {
                rocket_cache_weakpoint(t->unit,root,point);
                memcpy(t->position,point,sizeof(point));
            }
        }
    }
    if (active_flight) rocket_update_flights(mod,dt);
    if (native_flight) rocket_update_native(mod,dt);
    if (!rocket_kind) { atomic_store(&rocket_ui_state,0);rocket_network_tick(mod,dt);return; }
    const bool guide=rocket_pressed(rocket_guide_key), function=rocket_pressed(rocket_function_key), reload=rocket_pressed(rocket_reload_key);
    if (rocket_kind==2 && !rocket_reload_time) {
        if (guide && !rocket_previous_guide) { rocket_guidance=!rocket_guidance;rocket_lock_time=0;rocket_lock=0; }
        if (function && !rocket_previous_function) rocket_function=(rocket_function+1)%4;
    }
    rocket_previous_guide=guide;rocket_previous_function=function;
    RocketTarget acquired={0};
    RocketTarget* target=NULL;
    if ((rocket_kind==1 || rocket_guidance) &&
        rocket_native_lock(rocket_weapon,&acquired.entity,&acquired.node,acquired.position)) {
        acquired.unit=optical_entity_unit(api,(uintptr_t)hf_global_EntityManager(mod),acquired.entity,NULL);
        if (rocket_distance2(origin,acquired.position)<=600.0f*600.0f) target=&acquired;
    }
    const uint32_t candidate=target ? target->entity : 0;
    if (!candidate || candidate!=rocket_lock_candidate) { rocket_lock_candidate=candidate;rocket_lock_time=0;rocket_lock=0; }
    else { rocket_lock_time+=dt;if (rocket_lock_time>=0.35f) rocket_lock=candidate; }
    atomic_store(&rocket_ui_target_unit,target?target->unit:0);
    atomic_store(&rocket_ui_target_entity,target?target->entity:0);
    for (unsigned j=0;j<3;++j) atomic_store(&rocket_ui_point[j],target?target->position[j]:0);
    atomic_store(&rocket_ui_progress,fminf(1.0f,rocket_lock_time/0.35f));
    atomic_store(&rocket_ui_distance,target?sqrtf(rocket_distance2(origin,target->position)):0);
    uint32_t mag=0,pack=0,reserve=0;
    if (rocket_ammunition(rocket_weapon,&mag,&pack,&reserve)) {
        if (rocket_kind==2 && reload && !rocket_previous_reload && pack && mag<4 && reserve && !rocket_reload_time) {
            rocket_reload_time=1.4f;
            rocket_reload_audio();
        }
        if (rocket_reload_time>0 && (rocket_reload_time-=dt)<=0) {
            rocket_reload_time=0;
            if (pack && rocket_set_magazine) {
                const uint32_t amount=reserve<4-mag ? reserve : 4-mag;
                if (amount && rocket_set_reserve(pack,reserve,reserve-amount)) {
                    if (rocket_set_loaded(rocket_weapon,mag,mag+amount)) {
                        reserve-=amount;mag+=amount;
                    } else {
                        rocket_set_reserve(pack,reserve-amount,reserve);
                        api->log(mod,"Rockets: reload cancelled; magazine changed before transfer");
                    }
                }
            }
        }
        rocket_pack_rounds=reserve;rocket_mag_rounds=mag;
    }
    rocket_previous_reload=reload;
    uint32_t ui=rocket_kind | ((rocket_kind==1 || rocket_guidance)?4:0) | (rocket_function<<3) |
        (rocket_lock?0x40:0) | (candidate?0x80:0) | (rocket_reload_time>0?0x100:0) |
        (rocket_ready && rocket_native_ready && rocket_assets && rocket_hooks &&
            (rocket_kind!=2 || rocket_function!=ROCKET_AIRBURST || rocket_airburst_assets)?0x200:0);
    atomic_store(&rocket_ui_count,rocket_mag_rounds|(rocket_pack_rounds<<8));
    atomic_store(&rocket_ui_state,ui);
    rocket_network_tick(mod,dt);
}

static void rocket_rows(HfMod* mod,HfEvent event,void* user)
{
    (void)event;(void)user;
    const HfApi* api=HF_API(mod);
    rocket_ready=false;
    const int damage[]={1300,1400,3200,300};
    for (unsigned i=0;i<4;++i) {
        HfDamageInfo* d=(HfDamageInfo*)api->damage_row(rocket_damage[i]);
        if (!d || d->id!=rocket_damage[i]) return;
        d->standard_damage=d->durable_damage=damage[i];
        d->demolition=i==2?40:i==3?0:30;
        for (unsigned k=0;k<4;++k) d->armor_penetration[k]=i==3?3:5;
    }
    HfDamageInfo* contact=(HfDamageInfo*)api->damage_row(rocket_contact_damage);
    if (!contact || contact->id!=rocket_contact_damage || !rocket_blast_install(mod,rocket_damage)) return;
    memset((uint8_t*)contact+4,0,sizeof(*contact)-4);
    for (unsigned i=0;i<5;++i) {
        HfProjectileSettings* p=(HfProjectileSettings*)api->projectile_row(rocket_projectiles[i]);
        if (!p || p->type!=rocket_projectiles[i]) return;
        p->damage=rocket_contact_damage;
        p->velocity=i==4?55.0f:180.0f;
        p->gravity=i==4?0.75f:0;
        p->drag=0;
        p->lifetime=i==4?0.8f:8.0f;
        p->pellet_count=1;
        p->impact_explosion=i==0?424:i==1?425:i==2?426:i==3?425:427;
        p->expiry_explosion=i==4?427:0;
    }
    rocket_ready=true;
    api->log(mod,"Rockets: four private blast profiles, AP5/AP3; 180 m/s direct rockets, 300 RPM");
}

static void rocket_entities_ready(HfMod* mod,HfEvent event,void* user)
{
    (void)event;(void)user;
    const HfApi* api=HF_API(mod);
    rocket_native_ready=false;
    const HfSeekingMissile* donor=hf_record(mod,ROCKET_SPEAR_MISSILE,HfSeekingMissile);
    if (!donor) return;
    for (unsigned i=0;i<5;++i) {
        HfSeekingMissile* missile=hf_record(mod,rocket_missiles[i],HfSeekingMissile);
        const uint32_t row=rocket_projectiles[i==4?1:i];
        /* 6414CD..6414EB: a missile's +B0 hit-row override wins over spawn
         * info. Own it per variant so remote missiles keep the right warhead. */
        if (!missile || missile==donor || api->write((uintptr_t)missile+0xB0,&row,4)) {
            api->log(mod,"Rockets: private Spear missile %u unavailable; firing disabled",i);return;
        }
    }
    for (unsigned i=0;i<2;++i) {
        const HfSensorEye* sensor=hf_record(mod,rocket_weapons[i],HfSensorEye);
        const HfProjectileWeapon* weapon=hf_record(mod,rocket_weapons[i],HfProjectileWeapon);
        if (!sensor || sensor->range!=600 || !weapon || weapon->projectile_entity!=rocket_missiles[i]) return;
    }
    rocket_native_ready=true;
    api->log(mod,"Rockets: native Spear scope, acquisition, target nodes and persistent seeking; range 600 m");
}

static void rocket_bind(HfMod* mod,HfEvent event,void* user)
{
    (void)event;(void)user;
    const HfApi* api=HF_API(mod);
    const uintptr_t game=api->module_base("game.dll",NULL);
    if (!game || !api->code_matches(game+0x615940,"4C 8B DC 55 53 56 57 41 55 41 56 41 57") ||
        !api->code_matches(game+0xFD9BA0,"48 83 EC 08 4C 8B 1D") ||
        !api->code_matches(game+0xFD9AF0,"48 83 EC 08") ||
        !api->code_matches(game+0x916FA0,"40 57 41 54 41 57 48 83 EC 70") ||
        !api->code_matches(game+0x6414D2,"8B 87 B0 00 00 00 85 C0") ||
        !api->code_matches(game+0x515100,"48 83 EC 28 4C 8B D1 48 85 C9") ||
        !api->code_matches(game+0x615B15,"49 8B 5D 28") ||
        !api->code_matches(game+0x615B30,"49 8B 85 48 02 00 00") ||
        !api->code_matches(game+0x13A9830,"40 55 56 41 55 41 56 48 8D AC 24 08 FF FF FF") ||
        !api->code_matches(game+0x742900,"48 89 5C 24 20 55 56 57 48 83 EC 20") ||
        !api->code_matches(game+0x744690,"48 89 4C 24 08 53 55 56 57 41 55 41 57") ||
        !api->code_matches(game+0x744AD0,"48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20") ||
        !api->code_matches(game+0x76E2A0,"41 56 48 83 EC 20") ||
        !api->code_matches(game+0x13A8700,"48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57") ||
        !api->code_matches(game+0x87FE30,"89 1C F7 48 8B 45 38") ||
        !api->code_matches(game+0x87FE42,"4C 8D 04 F0") ||
        !api->code_matches(game+0x87FE5A,"E9 81 99 75 00")) {
        api->log(mod,"Rockets: native code differs; rocket firing disabled");return;
    }
    if (api->hook_ex(mod,game+0x13A9830,(void*)rocket_realize,(void**)&rocket_realize_next,0) ||
        api->hook_ex(mod,game+0x615940,(void*)rocket_fire,(void**)&rocket_pellet_next,0) ||
        api->hook_ex(mod,game+0x742900,(void*)rocket_round_count,(void**)&rocket_count_next,0) ||
        api->hook_ex(mod,game+0x744AD0,(void*)rocket_can_fire,(void**)&rocket_can_fire_next,0) ||
        api->hook_ex(mod,game+0x744690,(void*)rocket_consume,(void**)&rocket_consume_next,0)) return;
    rocket_set_magazine=(RocketSetMagazine)(game+0x76E2A0);
    rocket_live_settings=(uintptr_t (*)(const void*))(game+0x515100);
    rocket_replicate=(RocketReplicate)(game+0xFD97E0);
    rocket_handle_entity=(uint32_t* (*)(uint32_t*,uint32_t))(game+0xFD9BA0);
    rocket_entity_handle=(uint32_t (*)(uint32_t))(game+0xFD9AF0);
    rocket_target_point=(float* (*)(void*,float*,const void*,uint32_t))(game+0x916FA0);
    rocket_hooks=true;
}

static bool rocket_install(HfMod* mod,uint32_t heading)
{
    const HfApi* api=HF_API(mod);
    rocket_mod=mod;
    rocket_target_names(api);
    wchar_t ini[32768];
    const char* folder=api->mod_folder(mod);
    int n=folder?MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,folder,-1,ini,32768):0;
    if (n && n+16<32768) {
        memcpy(ini+n-1,L"\\rockets.ini",sizeof(L"\\rockets.ini"));
        rocket_guide_key=(int)GetPrivateProfileIntW(L"Keys",L"Guidance",VK_F6,ini);
        rocket_function_key=(int)GetPrivateProfileIntW(L"Keys",L"Warhead",VK_F7,ini);
        rocket_reload_key=(int)GetPrivateProfileIntW(L"Keys",L"Reload",'R',ini);
        for (unsigned i=0;i<2;++i) rocket_cooldowns[i]=(float)GetPrivateProfileIntW(
            L"Stratagems",i?L"SmartCooldown":L"Type2Cooldown",i?480:120,ini);
    }
    if (rocket_guide_key<1 || rocket_guide_key>255) rocket_guide_key=VK_F6;
    if (rocket_function_key<1 || rocket_function_key>255) rocket_function_key=VK_F7;
    if (rocket_reload_key<1 || rocket_reload_key>255) rocket_reload_key='R';
    atomic_store(&rocket_ui_keys,(uint32_t)rocket_guide_key|((uint32_t)rocket_function_key<<8)|((uint32_t)rocket_reload_key<<16));
    static const char* damage_names[]={ROCKET_PREFIX"type2_blast",ROCKET_PREFIX"smart_blast",ROCKET_PREFIX"focus_blast",ROCKET_PREFIX"bomblet_blast"};
    static const char* projectile_names[]={ROCKET_PREFIX"type2_round",ROCKET_PREFIX"smart_round",ROCKET_PREFIX"focus_round",ROCKET_PREFIX"airburst_round",ROCKET_PREFIX"bomblet_round"};
    for (unsigned i=0;i<4;++i) {
        HfDamage d={.size=sizeof(d),.base=563,.name=damage_names[i]};
        if (!(rocket_damage[i]=api->add_damage(mod,&d))) return false;
    }
    HfDamage contact={.size=sizeof(contact),.base=95,.name=ROCKET_PREFIX"contact_only"};
    if (!(rocket_contact_damage=api->add_damage(mod,&contact))) return false;
    for (unsigned i=0;i<5;++i) {
        HfProjectile p={.size=sizeof(p),.base=HF_PROJECTILE_EAT17_EXPENDABLE_ANTITANK,
            .name=projectile_names[i],.damage=rocket_contact_damage};
        if (!(rocket_projectiles[i]=api->add_projectile(mod,&p))) return false;
    }
    static const char* names[]={ROCKET_PREFIX"type2_launcher",ROCKET_PREFIX"smart_launcher"};
    static const char* missiles[]={ROCKET_PREFIX"type2_missile",ROCKET_PREFIX"smart_missile",
        ROCKET_PREFIX"focus_missile",ROCKET_PREFIX"airburst_missile",ROCKET_PREFIX"salvo_missile"};
    for (unsigned i=0;i<5;++i) {
        const HfEntity missile={.size=sizeof(missile),.base=ROCKET_SPEAR_MISSILE,.name=missiles[i],.own_all=1};
        rocket_missiles[i]=api->add_entity(mod,&missile);
        if (!rocket_missiles[i] || hf_set(mod,rocket_missiles[i],SeekingMissile,max_lifetime,HF_F32(12.0f))) return false;
    }
    static const char* titles[]={"次式实验性制导火箭弹","智能火箭筒"};
    static const char* descriptions[]={
        "一次性四联装反坦克发射器。锁定六百米内的中型及大型目标后发射制导火箭，优先攻击装甲薄弱部位。四次经过科学批准的民主投递，无需回收。",
        "配备弹药背包的四联装智能反坦克系统。可独立选择锁定或直射，以及单发、齐射、集中或空爆程序。锁定后发射，无需持续瞄准。背包携弹二十四发，支持站姿装填。每一项选择都能让敌人充分领会自由。"
    };
    for (unsigned w=0;w<2;++w) {
        HfEntity e={.size=sizeof(e),.base=HF_ENTITY_FAF14_SPEAR,.name=names[w],
            .own_all=1,.title=titles[w],.description=descriptions[w]};
        rocket_weapons[w]=api->add_entity(mod,&e);
        if (!rocket_weapons[w] ||
            hf_set(mod,rocket_weapons[w],SensorEye,range,HF_F32(600.0f)) ||
            hf_set(mod,rocket_weapons[w],ProjectileWeapon,projectile,HF_U32(rocket_projectiles[w])) ||
            hf_set(mod,rocket_weapons[w],ProjectileWeapon,projectile_entity,HF_U64(rocket_missiles[w])) ||
            hf_set(mod,rocket_weapons[w],ProjectileWeapon,weapon_function_projectile_type,HF_U32(rocket_projectiles[w])) ||
            hf_set(mod,rocket_weapons[w],WeaponMagazine,capacity,HF_U32(w?5:4)) ||
            hf_set(mod,rocket_weapons[w],WeaponMagazine,magazine_type,HF_U32(0)) ||
            hf_set(mod,rocket_weapons[w],WeaponMagazine,first_projectile,HF_U32(rocket_projectiles[w])) ||
            hf_set(mod,rocket_weapons[w],WeaponMagazine,chambered,HF_U8(0)) ||
            hf_set(mod,rocket_weapons[w],WeaponMagazine,starting_magazines,HF_U32(0)) ||
            hf_set(mod,rocket_weapons[w],WeaponMagazine,max_magazines,HF_U32(0)) ||
            hf_set(mod,rocket_weapons[w],WeaponData,weapon_function_left,HF_U32(0)) ||
            hf_set(mod,rocket_weapons[w],WeaponData,weapon_function_right,HF_U32(0))) return false;
        for (unsigned i=0;i<32;++i) {
            const HfField f=HF_FIELD_AT(WeaponMagazine,pattern,i);
            const HfValue v=HF_U32(rocket_projectiles[w]);
            if (api->set_field(mod,rocket_weapons[w],&f,&v)) return false;
        }
        for (unsigned i=0;i<3;++i) {
            HfField f=HF_FIELD_AT(ProjectileWeapon,fire_rate,i);
            HfValue v=HF_F32(300);
            if (api->set_field(mod,rocket_weapons[w],&f,&v)) return false;
        }
    }
    HfEntity p={.size=sizeof(p),.base=HF_ENTITY_BELTFED_GRENADE_LAUNCHER_BACKPACK,
        .name=ROCKET_PREFIX"smart_backpack",.own_all=1,.title="智能火箭筒弹药背包",
        .description="携带二十四枚智能反坦克火箭弹。专供智能火箭筒补充弹药。"};
    rocket_pack=api->add_entity(mod,&p);
    if (!rocket_pack || hf_set(mod,rocket_pack,Deposit,capacity,HF_U32(24)) ||
        hf_set(mod,rocket_pack,Deposit,start_amount,HF_I32(24)) ||
        hf_set(mod,rocket_pack,Deposit,refill_amount,HF_U32(12)) ||
        hf_set(mod,rocket_pack,Deposit,assisted_reload_weapon,HF_U64(rocket_weapons[1]))) return false;
    for (unsigned w=0;w<2;++w) {
        HfEntity rack={.size=sizeof(rack),.base=ROCKET_RACK,
            .name=w?ROCKET_PREFIX"smart_rack":ROCKET_PREFIX"type2_rack",.own_all=1};
        const HfRef r=api->add_entity(mod,&rack);
        if (!r || hf_set(mod,r,HellpodRack,slot0_item,HF_U64(rocket_weapons[w])) ||
            hf_set(mod,r,HellpodRack,slot1_item,HF_U64(w?rocket_pack:0))) return false;
        static uint64_t payloads[2][2];
        payloads[w][0]=r;payloads[w][1]=ROCKET_POD;
        HfStratagem s={.size=sizeof(s),.base=HF_STRATAGEM_MLS4X_COMMANDO,
            .name=w?ROCKET_PREFIX"smart":ROCKET_PREFIX"type2",
            .title=titles[w],.description=descriptions[w],
            .code=w?"DDULRURRL":"DDULRURLU",.cooldown=rocket_cooldowns[w],.uses=-1,
            .payload=payloads[w],.payload_count=2,.package=ROCKET_SPEAR_PACKAGE,
            .loads_item=HF_ITEM_FAF14_SPEAR,
            .flags=HF_TRAIT_SUPPORT_WEAPON|HF_TRAIT_HELLPOD|HF_TRAIT_ANTI_TANK|
                (w?HF_TRAIT_BACKPACK:HF_TRAIT_EXPENDABLE),
            .selection_category=heading,.selection_order=(int32_t)w+2};
        if (!api->add_stratagem(mod,&s)) return false;
    }
    return api->subscribe(mod,HF_ON_MESSAGE,rocket_message,NULL,0) &&
        !hf_on_spawn(mod,rocket_native_spawn,NULL,0) &&
        !hf_on_entities_ready(mod,rocket_entities_ready,NULL) &&
        !hf_on_projectiles_ready(mod,rocket_rows,NULL) &&
        !hf_on_game_loaded(mod,rocket_bind,NULL) &&
        !hf_on_weapon(mod,rocket_weapon_changed,NULL,0) &&
        !hf_on_game_thread(mod,rocket_tick,NULL);
}
