#pragma once

/* Private primitive rounds retain the game's collision, impact and kill credit.
 * Only their flight direction and the local TV camera are controlled here. */
static HfRef rocket_special_weapons[2],rocket_special_packs[2];
static uint32_t rocket_special_projectiles[2],rocket_special_damage[2];
static uint32_t rocket_special_held,rocket_tv_slot,rocket_tv_generation;
static bool rocket_special_ready;
static int rocket_tv_exit_key=VK_F10;
static float rocket_special_reload,rocket_tv_send_clock;
static bool rocket_special_reload_down,rocket_special_guide_down,rocket_special_exit_down;
static bool rocket_sky_guidance;
static _Atomic uint32_t rocket_tv_control,rocket_tv_owner,rocket_tv_cancel;
static _Atomic ULONGLONG rocket_tv_camera_ack,rocket_tv_capture_until,rocket_tv_started;
static _Atomic float rocket_tv_view[7];
static _Atomic LONG rocket_tv_mouse_x,rocket_tv_mouse_y;
static _Atomic uint32_t rocket_tv_shift_mask,rocket_tv_boost_presses;
static _Atomic bool rocket_tv_boosted;
static _Atomic float rocket_tv_speed_view;
static _Atomic bool rocket_tv_input_ready;
static _Atomic bool rocket_tv_foreground;
static POINT rocket_tv_mouse_previous;
static bool rocket_tv_mouse_positioned;
static HANDLE rocket_tv_input_thread;
static struct { _Atomic float point[3]; _Atomic uint32_t count; } rocket_sky_ui[6];
static _Atomic uint32_t rocket_sky_inflight;
typedef struct RocketTvMessage {
    uint32_t magic,weapon,seed,action,target,boost;
    float direction[3];
} RocketTvMessage;
static struct { uint64_t peer; RocketTvMessage command; ULONGLONG expires; } rocket_tv_peers[4];

static bool rocket_tv_stale(ULONGLONG now,ULONGLONG started,ULONGLONG ack)
{
    return ack ? now-ack>750 : now-started>2000;
}

static bool rocket_tv_focus(void)
{
    DWORD process=0;
    GetWindowThreadProcessId(GetForegroundWindow(),&process);
    const bool focused=process==GetCurrentProcessId();
    atomic_store(&rocket_tv_foreground,focused);
    return focused;
}

static bool rocket_tv_capture(void)
{
    return atomic_load(&rocket_tv_control) &&
        GetTickCount64()<atomic_load(&rocket_tv_capture_until) && atomic_load(&rocket_tv_foreground);
}

static LRESULT CALLBACK rocket_tv_keyboard(int code,WPARAM message,LPARAM data)
{
    if (code==HC_ACTION) {
        const KBDLLHOOKSTRUCT* key=(const KBDLLHOOKSTRUCT*)data;
        if (!(key->flags&LLKHF_INJECTED) &&
            (key->vkCode==VK_SHIFT || key->vkCode==VK_LSHIFT || key->vkCode==VK_RSHIFT)) {
            const uint32_t bit=key->vkCode==VK_RSHIFT?2:1;
            const bool capture=rocket_tv_capture();
            if (key->flags&LLKHF_UP) atomic_fetch_and(&rocket_tv_shift_mask,~bit);
            else if (!(atomic_fetch_or(&rocket_tv_shift_mask,bit)&bit) && capture)
                atomic_fetch_add(&rocket_tv_boost_presses,1);
            if (capture) return 1;
        }
    }
    if (code==HC_ACTION && rocket_tv_capture()) {
        const KBDLLHOOKSTRUCT* key=(const KBDLLHOOKSTRUCT*)data;
        if (!(key->flags&LLKHF_INJECTED) && !(key->flags&LLKHF_UP)) {
            if (key->vkCode==(DWORD)rocket_tv_exit_key || key->vkCode==VK_ESCAPE)
                atomic_store(&rocket_tv_cancel,atomic_load(&rocket_tv_control));
            switch (key->vkCode) {
                case 'W':case 'A':case 'S':case 'D':case 'R':case 'E':case 'F':
                case '1':case '2':case '3':case '4':case '5':case VK_SPACE:
                case VK_SHIFT:case VK_LSHIFT:case VK_RSHIFT:
                    return 1;
                default:break;
            }
            if (key->vkCode==(DWORD)rocket_tv_exit_key || key->vkCode==VK_ESCAPE) return 1;
        }
    }
    return CallNextHookEx(NULL,code,message,data);
}

static LRESULT CALLBACK rocket_tv_mouse(int code,WPARAM message,LPARAM data)
{
    if (code==HC_ACTION) {
        const MSLLHOOKSTRUCT* mouse=(const MSLLHOOKSTRUCT*)data;
        const bool capture=rocket_tv_capture();
        if (message==WM_MOUSEMOVE) {
            const LONG dx=mouse->pt.x-rocket_tv_mouse_previous.x,dy=mouse->pt.y-rocket_tv_mouse_previous.y;
            /* Low-level hooks must not synchronously query another window/input thread. */
            if (capture && rocket_tv_mouse_positioned && !(mouse->flags&LLMHF_INJECTED) &&
                labs(dx)<4096 && labs(dy)<4096) {
                atomic_fetch_add(&rocket_tv_mouse_x,dx);
                atomic_fetch_add(&rocket_tv_mouse_y,dy);
            }
            /* A swallowed move does not advance the OS cursor position. */
            if (!capture || (mouse->flags&LLMHF_INJECTED)) {
                rocket_tv_mouse_previous=mouse->pt;
                rocket_tv_mouse_positioned=true;
            }
            if (capture && !(mouse->flags&LLMHF_INJECTED)) return 1;
        }
        if (!capture) return CallNextHookEx(NULL,code,message,data);
        if (!(mouse->flags&LLMHF_INJECTED)) {
            if (message==WM_LBUTTONDOWN || message==WM_RBUTTONDOWN || message==WM_MOUSEWHEEL) return 1;
        }
    }
    return CallNextHookEx(NULL,code,message,data);
}

static DWORD WINAPI rocket_tv_input_run(void* unused)
{
    (void)unused;
    HMODULE self=NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)rocket_tv_input_run,&self);
    rocket_tv_mouse_positioned=GetCursorPos(&rocket_tv_mouse_previous)!=0;
    HHOOK keyboard=SetWindowsHookExA(WH_KEYBOARD_LL,rocket_tv_keyboard,self,0);
    HHOOK mouse=SetWindowsHookExA(WH_MOUSE_LL,rocket_tv_mouse,self,0);
    atomic_store(&rocket_tv_input_ready,keyboard && mouse);
    MSG message;
    while (GetMessageA(&message,NULL,0,0)>0) {
        TranslateMessage(&message);DispatchMessageA(&message);
    }
    if (keyboard) UnhookWindowsHookEx(keyboard);
    if (mouse) UnhookWindowsHookEx(mouse);
    atomic_store(&rocket_tv_input_ready,false);
    return 0;
}

static unsigned rocket_special_index(HfRef resource)
{
    for (unsigned i=0;i<2;++i) if (resource && resource==rocket_special_weapons[i]) return i+1;
    return 0;
}

static uint32_t rocket_special_pack(uint32_t avatar,unsigned kind)
{
    const HfApi* api=HF_API(rocket_mod);
    const uintptr_t inventory=(uintptr_t)hf_global_InventoryManager(rocket_mod);
    uint32_t index,pack=0;
    if (!kind || kind>2 || !rocket_map(inventory,0x28,avatar,&index)) return 0;
    const uintptr_t records=optical_ptr(api,inventory+0x50);
    HfRef resource=0;
    if (!records || api->read(records+index*0x30+0xC,&pack,4) ||
        !optical_entity_unit(api,(uintptr_t)hf_global_EntityManager(rocket_mod),pack,&resource)) return 0;
    return resource==rocket_special_packs[kind-1] ? pack : 0;
}

static unsigned rocket_special_capacity(unsigned kind) { return kind==1?2:6; }
static unsigned rocket_special_reserve(unsigned kind) { return kind==1?10:36; }

static bool rocket_special_ammo(uint32_t weapon,uint32_t avatar,unsigned kind,
    uint32_t* loaded,uint32_t* pack,uint32_t* spare)
{
    const HfApi* api=HF_API(rocket_mod);
    const uintptr_t game=api->module_base("game.dll",NULL);
    const uintptr_t magazines=optical_ptr(api,game+0x3326648);
    uint32_t index,physical=0;
    if (!rocket_map(magazines,0x20,weapon,&index)) return false;
    const uintptr_t records=optical_ptr(api,magazines+0x48);
    if (!records || api->read(records+index*16,&physical,4) ||
        !physical || physical>rocket_special_capacity(kind)+1) return false;
    *loaded=physical-1;*spare=0;*pack=rocket_special_pack(avatar,kind);
    if (!*pack) return true;
    const uintptr_t deposit=optical_ptr(api,game+0x33265E8);
    if (!rocket_map(deposit,0x20,*pack,&index)) return false;
    const uintptr_t counts=optical_ptr(api,deposit+0x50);
    return counts && !api->read(counts+index*8,spare,4) && *spare<=rocket_special_reserve(kind);
}

static bool rocket_special_reload_finish(unsigned kind,uint32_t magazine,uint32_t pack,uint32_t spare)
{
    const HfApi* api=HF_API(rocket_mod);
    const uintptr_t game=api->module_base("game.dll",NULL);
    const uintptr_t deposits=optical_ptr(api,game+0x33265E8),mags=optical_ptr(api,game+0x3326648);
    uint32_t di,mi,old_mag=0,old_spare=0;
    const uint32_t needed=rocket_special_capacity(kind)-magazine;
    const uint32_t transfer=spare<needed?spare:needed;
    if (!transfer || !rocket_map(deposits,0x20,pack,&di) || !rocket_map(mags,0x20,rocket_weapon,&mi)) return false;
    const uintptr_t counts=optical_ptr(api,deposits+0x50),records=optical_ptr(api,mags+0x48);
    const uintptr_t descriptors=optical_ptr(api,deposits+0x38);
    const uintptr_t descriptor=descriptors?optical_ptr(api,descriptors+di*8):0;
    uint32_t network=UINT32_MAX;
    if (!counts || !records || !descriptor || !rocket_set_magazine || !rocket_replicate ||
        api->read(counts+di*8,&old_spare,4) || old_spare!=spare ||
        api->read(records+mi*16,&old_mag,4) || old_mag!=magazine+1 ||
        api->read(descriptor+0x10,&network,4)) return false;
    const uint32_t remaining=spare-transfer,physical=magazine+transfer+1;
    if (api->write(counts+di*8,&remaining,4)) return false;
    if (api->write(records+mi*16,&physical,4)) {
        api->write(counts+di*8,&spare,4);return false;
    }
    rocket_replicate(network,0x69889752u,(const void*)(counts+di*8));
    rocket_set_magazine(NULL,rocket_weapon,physical);
    return true;
}

static void rocket_tv_release(const char* reason)
{
    const uint32_t generation=atomic_load(&rocket_tv_control);
    atomic_store(&rocket_tv_capture_until,0);
    atomic_store(&rocket_tv_control,0);
    atomic_store(&rocket_tv_owner,0);
    atomic_store(&rocket_tv_cancel,0);
    atomic_store(&rocket_tv_mouse_x,0);atomic_store(&rocket_tv_mouse_y,0);
    atomic_store(&rocket_tv_boost_presses,0);
    atomic_store(&rocket_tv_boosted,false);
    atomic_store(&rocket_tv_speed_view,30);
    rocket_tv_slot=0;
    if (generation && rocket_mod)
        HF_API(rocket_mod)->log(rocket_mod,"TV control %u: released (%s)",generation,reason);
}

static void rocket_special_reset(void)
{
    rocket_tv_release("mission reset");
    rocket_special_held=0;
    rocket_sky_guidance=false;
    if (rocket_special_reload>0) rocket_reload_audio_stop();
    rocket_special_reload=0;
    rocket_special_reload_down=rocket_special_guide_down=rocket_special_exit_down=false;
    memset(rocket_tv_peers,0,sizeof(rocket_tv_peers));
    atomic_store(&rocket_sky_inflight,0);
    for (unsigned i=0;i<6;++i) atomic_store(&rocket_sky_ui[i].count,0);
}

static void rocket_special_weapon(const HfWeaponEvent* event)
{
    /* This callback runs after rocket_weapon has been updated. */
    if (rocket_tv_slot && rocket_flights[rocket_tv_slot-1].source!=event->entity) rocket_tv_release("weapon changed");
    if (rocket_special_reload>0) rocket_reload_audio_stop();
    rocket_special_reload=0;
    rocket_special_held=rocket_special_index(event->resource);
    rocket_special_reload_down=(GetAsyncKeyState(rocket_reload_key)&0x8000)!=0;
    rocket_special_guide_down=(GetAsyncKeyState(rocket_guide_key)&0x8000)!=0;
    rocket_special_exit_down=(GetAsyncKeyState(rocket_tv_exit_key)&0x8000)!=0;
}

static bool rocket_special_allowed(uint32_t weapon)
{
    if (!rocket_special_ready || !rocket_ready || !rocket_hooks || !rocket_assets) return false;
    if (weapon!=rocket_weapon) return true;
    if (!rocket_special_held || rocket_special_reload>0 || atomic_load(&rocket_tv_control)) return false;
    if (rocket_special_held==1 &&
        (!atomic_load(&rocket_tv_input_ready) || GetTickCount64()-atomic_load(&rocket_camera_at)>350)) return false;
    if (rocket_special_held==2 && rocket_sky_guidance) {
        uint32_t target,node;
        float point[3];
        uint8_t locked=0;
        const uintptr_t instance=rocket_native_instance(weapon);
        if (!instance || HF_API(rocket_mod)->read(instance+8,&locked,1) || !locked ||
            !rocket_native_lock(weapon,&target,&node,point)) return false;
    }
    uint32_t mag,pack,spare;
    return rocket_special_ammo(weapon,rocket_avatar,rocket_special_held,&mag,&pack,&spare) && mag>0;
}

static bool rocket_special_fire(void* manager,uint32_t index,const uint8_t* transform,
    uint32_t owner,float a6,uint32_t seed,uint8_t a8,float a9,float a10,float a11,uint8_t a12,
    uint32_t source,HfRef resource)
{
    const unsigned kind=rocket_special_index(resource);
    if (!kind) return false;
    rocket_pending_cost=0;rocket_pending_weapon=source;
    const HfApi* api=HF_API(rocket_mod);
    if (!owner || !transform || !rocket_special_allowed(source) ||
        api->game_state(rocket_mod)!=HF_STATE_MISSION) return true;
    if (kind==1 && source==rocket_weapon &&
        (!atomic_load(&rocket_tv_input_ready) || GetTickCount64()-atomic_load(&rocket_camera_at)>350)) return true;
    uint32_t target=0,node=16;
    float point[3];
    const bool guided=kind==2 && (source!=rocket_weapon || rocket_sky_guidance) &&
        rocket_native_lock(source,&target,&node,point);
    if (kind==2 && source==rocket_weapon && rocket_sky_guidance && !guided) return true;
    const uintptr_t descriptors=optical_ptr(api,(uintptr_t)manager+0x68);
    const uintptr_t descriptor=descriptors?optical_ptr(api,descriptors+index*8):0;
    const uintptr_t settings=descriptor && rocket_live_settings?rocket_live_settings((void*)descriptor):0;
    uint64_t old_entity,old_function,zero=0;
    if (!settings || api->read(settings+0x28,&old_entity,8) ||
        api->read(settings+0x248,&old_function,8) || api->write(settings+0x28,&zero,8)) return true;
    if (api->write(settings+0x248,&zero,8)) {api->write(settings+0x28,&old_entity,8);return true;}
    _Alignas(16) float shot[16];
    memcpy(shot,transform,sizeof(shot));
    if (kind==2 && !guided) {
        float forward[3]={shot[4]*0.22f,shot[5]*0.22f,1.0f};
        rocket_normalize(forward);
        float right[3]={forward[1],-forward[0],0};
        if (!rocket_normalize(right)) {right[0]=1;right[1]=right[2]=0;}
        const float up[3]={right[1]*forward[2]-right[2]*forward[1],
            right[2]*forward[0]-right[0]*forward[2],right[0]*forward[1]-right[1]*forward[0]};
        memcpy(shot,right,12);memcpy(shot+4,forward,12);memcpy(shot+8,up,12);
    }
    for (unsigned j=0;j<3;++j) shot[12+j]+=shot[4+j]*1.2f;
    memset(&rocket_shot,0,sizeof(rocket_shot));
    rocket_shot.active=true;rocket_shot.source=source;rocket_shot.seed=seed;
    rocket_shot.type=rocket_special_projectiles[kind-1];
    rocket_shot.homing=guided;rocket_shot.target=guided?target:0;rocket_shot.node=node;
    rocket_queue_shot();
    rocket_pending_cost=1;
    if (kind==1 && source==rocket_weapon)
        api->log(rocket_mod,"TV control: launch queued, weapon %u, projectile %u, seed %u",source,rocket_shot.type,seed);
    rocket_pellet_next(manager,index,rocket_shot.type,(const uint8_t*)shot,owner,a6,seed,a8,a9,a10,a11,a12);
    rocket_shot.active=false;
    api->write(settings+0x248,&old_function,8);api->write(settings+0x28,&old_entity,8);
    return true;
}

static void rocket_special_realized(unsigned slot,uint32_t source,uint32_t type,uint32_t seed)
{
    unsigned kind=0;
    for (unsigned i=0;i<2;++i) if (type==rocket_special_projectiles[i]) kind=i+1;
    if (!kind) return;
    RocketFlight* f=&rocket_flights[slot];
    const uint32_t shot_seed=f->active && f->source==source?f->seed:seed;
    const uint32_t preference=f->active && f->source==source?f->target:0;
    const bool guided=kind==2 && f->active && f->source==source && f->homing && preference;
    const uint32_t node=guided?f->node:16;
    *f=(RocketFlight){.active=true,.special=kind,.source=source,.type=type,.seed=shot_seed,
        .node=node,.preference=preference,.homing=guided,.terminal=guided,.target=guided?preference:0};
    if (kind==1 && source==rocket_weapon && rocket_special_held==1 &&
        atomic_load(&rocket_tv_input_ready)) {
        rocket_tv_release("new missile");
        rocket_tv_slot=slot+1;
        if (!++rocket_tv_generation) ++rocket_tv_generation;
        atomic_store(&rocket_tv_camera_ack,0);
        atomic_store(&rocket_tv_started,GetTickCount64());
        atomic_store(&rocket_tv_view[6],0);
        atomic_store(&rocket_tv_owner,rocket_avatar);
        atomic_store(&rocket_tv_control,rocket_tv_generation);
        HF_API(rocket_mod)->log(rocket_mod,"TV control %u: missile realized, slot %u, weapon %u",rocket_tv_generation,slot,source);
    }
}

static bool rocket_special_message(HfMod* mod,const HfMessageEvent* event)
{
    if (event->bytes!=sizeof(RocketTvMessage)) return false;
    RocketTvMessage packet;
    memcpy(&packet,event->data,sizeof(packet));
    if (packet.magic!=0x32565452u) return false;
    const HfApi* api=HF_API(mod);
    if (packet.action>2 || packet.boost>1 || api->game_state(mod)!=HF_STATE_MISSION) return true;
    const uintptr_t game=api->module_base("game.dll",NULL);
    const uint32_t avatar=optical_peer_avatar(mod,game,event->from);
    const uint32_t source=api->entity_of_network(mod,packet.weapon);
    HfRef resource=0;
    uint32_t index,support=0;
    const uintptr_t inventory=(uintptr_t)hf_global_InventoryManager(mod);
    if (!avatar || !rocket_map(inventory,0x28,avatar,&index)) return true;
    const uintptr_t records=optical_ptr(api,inventory+0x50);
    if (!records || api->read(records+index*0x30+8,&support,4) || support!=source ||
        !optical_entity_unit(api,(uintptr_t)hf_global_EntityManager(mod),source,&resource) ||
        resource!=rocket_special_weapons[0]) return true;
    for (unsigned i=0;i<3;++i) if (!isfinite(packet.direction[i])) return true;
    if (!rocket_normalize(packet.direction)) return true;
    unsigned at=4;
    for (unsigned i=0;i<4;++i) if (rocket_tv_peers[i].peer==event->from) {at=i;break;}
    if (at==4) for (unsigned i=0;i<4;++i)
        if (GetTickCount64()>=rocket_tv_peers[i].expires) {at=i;break;}
    if (at<4) {
        packet.weapon=source;
        packet.target=packet.target==UINT32_MAX?0:api->entity_of_network(mod,packet.target);
        rocket_tv_peers[at].peer=event->from;rocket_tv_peers[at].command=packet;
        rocket_tv_peers[at].expires=GetTickCount64()+750;
    }
    return true;
}

static void rocket_tv_send(const RocketFlight* flight,unsigned action)
{
    HfSession session={0};
    const HfApi* api=HF_API(rocket_mod);
    api->session(rocket_mod,&session);
    if (session.role==HF_SESSION_SOLO) return;
    RocketTvMessage packet={.magic=0x32565452u,.weapon=rocket_network_id(flight->source),
        .seed=flight->seed,.action=action,.boost=flight->boosted?1:0,
        .target=flight->target?rocket_network_id(flight->target):UINT32_MAX};
    memcpy(packet.direction,flight->direction,12);
    api->send(rocket_mod,HF_TO_ALL,&packet,sizeof(packet));
}

static RocketTarget* rocket_sky_assign(const float position[3],uint32_t preference,unsigned slot)
{
    RocketTarget* best=NULL;
    float score=1e30f;
    uint32_t allocated[64]={0},counts[64]={0};
    unsigned size=0;
    for (unsigned n=0;n<2048;++n) {
        const RocketFlight* f=&rocket_flights[n];
        if (n==slot || !f->active || f->special!=2 || !f->target) continue;
        unsigned at=size;
        for (unsigned j=0;j<size;++j) if (allocated[j]==f->target) {at=j;break;}
        if (at<64) {allocated[at]=f->target;++counts[at];if (at==size) ++size;}
    }
    for (unsigned i=0;i<rocket_target_count;++i) {
        RocketTarget* t=&rocket_targets[i];
        const float distance=rocket_distance2(position,t->position);
        if (distance>600.0f*600.0f || distance<12.0f*12.0f) continue;
        unsigned assigned=0;
        for (unsigned n=0;n<size;++n) if (allocated[n]==t->entity) {assigned=counts[n];break;}
        const float candidate=assigned*1000000.0f+distance+(2-t->rank)*16000.0f-
            (t->entity==preference?400000.0f:0);
        if (candidate<score) {score=candidate;best=t;}
    }
    return best;
}

static void rocket_special_steer(float current[3],const float want[3],float rate,float dt)
{
    const float dot=fminf(1,fmaxf(-1,current[0]*want[0]+current[1]*want[1]+current[2]*want[2]));
    const float angle=acosf(dot),step=fminf(angle,rate*fmaxf(0,fminf(dt,0.1f)));
    if (angle<0.00001f) return;
    float tangent[3];
    for (unsigned j=0;j<3;++j) tangent[j]=want[j]-current[j]*dot;
    if (!rocket_normalize(tangent)) {
        tangent[0]=current[1];tangent[1]=-current[0];tangent[2]=0;
        if (!rocket_normalize(tangent)) {tangent[0]=1;tangent[1]=tangent[2]=0;}
    }
    for (unsigned j=0;j<3;++j) current[j]=current[j]*cosf(step)+tangent[j]*sinf(step);
    rocket_normalize(current);
}

static float rocket_tv_speed_step(RocketFlight* flight,float dt)
{
    const float desired=flight->boosted || (flight->handed_off && flight->target)?90.0f:30.0f;
    if (!isfinite(flight->speed) || flight->speed<30 || flight->speed>90) flight->speed=30;
    const float step=(desired>flight->speed?120.0f:150.0f)*fmaxf(0,fminf(dt,0.1f));
    flight->speed+=fmaxf(-step,fminf(step,desired-flight->speed));
    return flight->speed;
}

static void rocket_special_flight(HfMod* mod,unsigned slot,uintptr_t flight,
    const float position[3],float velocity[3],float dt)
{
    RocketFlight* f=&rocket_flights[slot];
    const HfApi* api=HF_API(mod);
    if (!f->positioned) {
        memcpy(f->launch,position,12);memcpy(f->direction,velocity,12);
        if (!rocket_normalize(f->direction)) {f->active=false;return;}
        f->positioned=true;
    }
    bool controlled=f->special==1 && rocket_tv_slot==slot+1 && atomic_load(&rocket_tv_control);
    if (controlled) {
        const uint32_t generation=atomic_load(&rocket_tv_control);
        const uintptr_t game=api->module_base("game.dll",NULL);
        const uint32_t avatar=optical_avatar(mod,game);
        const ULONGLONG ack=atomic_load(&rocket_tv_camera_ack),now=GetTickCount64();
        const char* reason=NULL;
        if (f->source!=rocket_weapon) reason="weapon changed";
        else if (avatar!=atomic_load(&rocket_tv_owner)) reason="operator changed";
        else if (!rocket_tv_focus()) reason="focus lost";
        else if (atomic_load(&rocket_tv_cancel)==generation) reason="exit or camera rejected";
        else if (rocket_tv_stale(now,atomic_load(&rocket_tv_started),ack))
            reason=ack?"render heartbeat expired":"camera startup timed out";
        if (reason) {
            rocket_tv_send(f,2);rocket_tv_release(reason);controlled=false;
        } else {
            if (atomic_exchange(&rocket_tv_boost_presses,0)&1) f->boosted=!f->boosted;
            atomic_store(&rocket_tv_boosted,f->boosted);
            /* Turning is limited to 110 degrees/s; no hovering or instant U-turn. */
            const LONG mx=atomic_exchange(&rocket_tv_mouse_x,0),my=atomic_exchange(&rocket_tv_mouse_y,0);
            float yaw=atan2f(f->direction[0],f->direction[1]);
            float pitch=asinf(fminf(1,fmaxf(-1,f->direction[2])));
            const float limit=1.919862f*fminf(dt,0.1f);
            yaw+=fminf(limit,fmaxf(-limit,mx*0.0022f));
            pitch=fminf(1.52f,fmaxf(-1.52f,pitch-fminf(limit,fmaxf(-limit,my*0.0022f))));
            f->direction[0]=sinf(yaw)*cosf(pitch);f->direction[1]=cosf(yaw)*cosf(pitch);f->direction[2]=sinf(pitch);
            for (unsigned j=0;j<3;++j) {
                atomic_store(&rocket_tv_view[j],position[j]);
                atomic_store(&rocket_tv_view[j+3],f->direction[j]);
            }
            atomic_store(&rocket_tv_view[6],f->age);
            if (ack && now-ack<350 && f->distance>4) atomic_store(&rocket_tv_capture_until,now+250);
            if ((rocket_tv_send_clock-=dt)<=0) {rocket_tv_send_clock=0.05f;rocket_tv_send(f,0);}
        }
    }
    if (f->special==1 && f->source!=rocket_weapon && !f->handed_off) {
        for (unsigned i=0;i<4;++i) {
            const RocketTvMessage* command=&rocket_tv_peers[i].command;
            if (rocket_tv_peers[i].expires<GetTickCount64() || command->weapon!=f->source || command->seed!=f->seed) continue;
            f->boosted=command->boost!=0;
            if (command->action==0) rocket_special_steer(f->direction,command->direction,1.919862f,dt);
            else if (command->action==1) {
                RocketTarget* target=rocket_find_target(command->target);
                if (target && target->rank && rocket_distance2(position,target->position)<=600.0f*600.0f) {
                    f->target=target->entity;f->node=target->node;f->handed_off=true;
                }
            } else f->handed_off=true;
            break;
        }
    }
    if (f->special==2) {
        const bool climbed=f->age>=0.9f && position[2]>=f->launch[2]+45;
        if (!f->terminal && (climbed || f->age>=1.5f)) f->terminal=true;
        if (!f->terminal) {
            float up[3]={f->direction[0]*0.12f,f->direction[1]*0.12f,1};
            rocket_normalize(up);rocket_special_steer(f->direction,up,2.4f,dt);
        } else if ((f->scan-=dt)<=0) {
            f->scan=0.15f;
            if (!rocket_find_target(f->target)) {
                RocketTarget* chosen=rocket_sky_assign(position,f->preference,slot);
                if (chosen) {f->target=chosen->entity;f->node=chosen->node;}
                else f->target=0;
            }
        }
        if (f->terminal && !f->target) {
            /* Cruise in a bounded search circle instead of climbing out of range. */
            const float phase=f->age*1.25f+(f->seed&255)*0.02454369f;
            float search[3]={f->launch[0]+cosf(phase)*45-position[0],
                f->launch[1]+sinf(phase)*45-position[1],f->launch[2]+70-position[2]};
            if (rocket_normalize(search)) rocket_special_steer(f->direction,search,3.2f,dt);
        }
    }
    if (!controlled && (f->special==2?f->terminal:f->handed_off) && f->target) {
        RocketTarget* target=rocket_find_target(f->target);
        float point[3];
        if (target) {
            if (!rocket_native_point(api,f->target,f->node,point)) memcpy(point,target->position,12);
            float aim[3];
            for (unsigned j=0;j<3;++j) aim[j]=point[j]-position[j];
            if (rocket_normalize(aim)) rocket_special_steer(f->direction,aim,f->special==2?4.2f:3.6f,dt);
        }
    }
    const float speed=f->special==1?rocket_tv_speed_step(f,dt):(f->terminal && f->target?120.0f:65.0f);
    if (controlled) atomic_store(&rocket_tv_speed_view,speed);
    for (unsigned j=0;j<3;++j) velocity[j]=f->direction[j]*speed;
    api->write(flight+12,velocity,12);
}

static void rocket_special_tick(HfMod* mod,float dt)
{
    (void)mod;
    if (rocket_tv_slot && !rocket_flights[rocket_tv_slot-1].active) rocket_tv_release("missile ended");
    const bool guide=rocket_tv_focus() && (GetAsyncKeyState(rocket_guide_key)&0x8000);
    const bool reload=rocket_tv_focus() && (GetAsyncKeyState(rocket_reload_key)&0x8000);
    const bool exit=rocket_tv_focus() && (GetAsyncKeyState(rocket_tv_exit_key)&0x8000);
    if (rocket_special_held==2 && guide && !rocket_special_guide_down && !rocket_special_reload)
        rocket_sky_guidance=!rocket_sky_guidance;
    RocketTarget acquired={0};
    RocketTarget* candidate=NULL;
    if (rocket_tv_slot) {
        RocketFlight* f=&rocket_flights[rocket_tv_slot-1];
        float position[3];
        for (unsigned j=0;j<3;++j) position[j]=atomic_load(&rocket_tv_view[j]);
        candidate=rocket_acquire(position,f->direction);
        if (guide && !rocket_special_guide_down && candidate) {
            f->target=candidate->entity;f->node=candidate->node;f->handed_off=true;
            rocket_tv_send(f,1);rocket_tv_release("terminal homing handed off");
        } else if (exit && !rocket_special_exit_down) {rocket_tv_send(f,2);rocket_tv_release("exit key");}
    }
    atomic_store(&rocket_sky_inflight,0);
    uint32_t targets[6]={0},counts[6]={0};
    unsigned total=0,unique=0;
    for (unsigned n=0;n<2048;++n) {
        RocketFlight* f=&rocket_flights[n];
        if (!f->active || f->special!=2 || f->source!=rocket_weapon) continue;
        ++total;
        if (!f->target) continue;
        unsigned at=unique;
        for (unsigned i=0;i<unique;++i) if (targets[i]==f->target) {at=i;break;}
        if (at<6) {targets[at]=f->target;++counts[at];if (at==unique) ++unique;}
    }
    atomic_store(&rocket_sky_inflight,total);
    for (unsigned i=0;i<6;++i) {
        RocketTarget* t=rocket_find_target(targets[i]);
        for (unsigned j=0;j<3;++j) atomic_store(&rocket_sky_ui[i].point[j],t?t->position[j]:0);
        atomic_store(&rocket_sky_ui[i].count,t?counts[i]:0);
    }
    if (rocket_special_held) {
        uint32_t mag=0,pack=0,spare=0;
        if (rocket_special_ammo(rocket_weapon,rocket_avatar,rocket_special_held,&mag,&pack,&spare)) {
            if (reload && !rocket_special_reload_down && !rocket_tv_slot && pack && spare &&
                mag<rocket_special_capacity(rocket_special_held) && !rocket_special_reload) {
                rocket_special_reload=1.4f;rocket_reload_audio();
            }
            if (rocket_special_reload>0 && (rocket_special_reload-=dt)<=0) {
                rocket_special_reload=0;
                rocket_special_reload_finish(rocket_special_held,mag,pack,spare);
                rocket_special_ammo(rocket_weapon,rocket_avatar,rocket_special_held,&mag,&pack,&spare);
            }
        }
        if (!candidate && rocket_special_held==1) {
            float origin[3],forward[3];
            for (unsigned j=0;j<3;++j) {origin[j]=atomic_load(&rocket_camera[j]);forward[j]=atomic_load(&rocket_camera[j+3]);}
            if (GetTickCount64()-atomic_load(&rocket_camera_at)<300) candidate=rocket_acquire(origin,forward);
        }
        if (rocket_special_held==2 && rocket_sky_guidance &&
            rocket_native_lock(rocket_weapon,&acquired.entity,&acquired.node,acquired.position)) {
            acquired.unit=optical_entity_unit(HF_API(rocket_mod),(uintptr_t)hf_global_EntityManager(rocket_mod),acquired.entity,NULL);
            candidate=&acquired;
        }
        uint8_t locked=0;
        const uintptr_t instance=candidate && rocket_special_held==2?rocket_native_instance(rocket_weapon):0;
        if (instance) HF_API(rocket_mod)->read(instance+8,&locked,1);
        const uint32_t state=(rocket_special_held==1?0x400:0x800) |
            (rocket_special_ready && rocket_ready && rocket_hooks && rocket_assets?0x200:0) |
            (rocket_special_reload>0?0x100:0) | (candidate?0x80:0) |
            (candidate && (rocket_special_held==1 || locked)?0x40:0) |
            (rocket_tv_slot?0x1000:0) | (rocket_special_held==1 || rocket_sky_guidance?4:0);
        atomic_store(&rocket_ui_state,state);atomic_store(&rocket_ui_count,mag|(spare<<8));
        atomic_store(&rocket_ui_target_unit,candidate?candidate->unit:0);
        atomic_store(&rocket_ui_target_entity,candidate?candidate->entity:0);
        atomic_store(&rocket_ui_progress,candidate?(rocket_special_held==1 || locked?1:0.5f):0);
        for (unsigned j=0;j<3;++j) atomic_store(&rocket_ui_point[j],candidate?candidate->position[j]:0);
        float origin[3];
        for (unsigned j=0;j<3;++j) origin[j]=rocket_tv_slot?
            atomic_load(&rocket_tv_view[j]):atomic_load(&rocket_camera[j]);
        atomic_store(&rocket_ui_distance,candidate?sqrtf(rocket_distance2(origin,candidate->position)):0);
    }
    rocket_special_reload_down=reload;rocket_special_guide_down=guide;rocket_special_exit_down=exit;
}

static void rocket_special_rows(HfMod* mod)
{
    const HfApi* api=HF_API(mod);
    rocket_special_ready=false;
    if (!rocket_blast_table) return;
    for (unsigned i=0;i<2;++i) {
        HfDamageInfo* d=(HfDamageInfo*)api->damage_row(rocket_special_damage[i]);
        HfProjectileSettings* p=(HfProjectileSettings*)api->projectile_row(rocket_special_projectiles[i]);
        if (!d || d->id!=rocket_special_damage[i] || !p || p->type!=rocket_special_projectiles[i]) return;
        d->standard_damage=d->durable_damage=i?520:2600;d->demolition=i?30:40;
        for (unsigned j=0;j<4;++j) d->armor_penetration[j]=5;
        HfExplosionSettings* blast=&rocket_blast_rows[5+i];
        blast->damage=rocket_special_damage[i];
        blast->inner_radius=blast->outer_radius=blast->shockwave_radius=i?1.2f:2.4f;
        blast->shrapnel_count=0;
        p->damage=rocket_contact_damage;p->velocity=i?65.0f:30.0f;
        p->gravity=p->drag=0;p->lifetime=16;p->pellet_count=1;
        p->impact_explosion=428+i;p->expiry_explosion=0;
    }
    rocket_special_ready=true;
    api->log(mod,"Rockets: TV 2/10 2600 AP5 2.4m demolition 40, Sky 6/36 520 AP5 1.2m demolition 30");
}

static bool rocket_special_install(HfMod* mod,uint32_t heading,const wchar_t* ini)
{
    const HfApi* api=HF_API(mod);
    if (ini) rocket_tv_exit_key=(int)GetPrivateProfileIntW(L"Keys",L"ExitControl",VK_F10,ini);
    if (rocket_tv_exit_key<1 || rocket_tv_exit_key>255) rocket_tv_exit_key=VK_F10;
    const char* titles[]={"数控火箭筒","“天穹”智能导弹系统"};
    const char* descriptions[]={
        "配备弹载成像终端的反坦克发射器。发射后由操作员亲自引导弹头，亦可锁定目标并移交末段攻击。双发待发，背包备弹十发。操作期间请寻求掩护，民主不提供代驾保险。",
        "六联装智能导弹系统。锁定程序追踪指定目标；直射程序无需锁定，导弹升空后自主分配六百米内的敌军并俯冲打击。配备三十六发弹药背包，让自由从天而降。"};
    const char* names[]={ROCKET_PREFIX"tv_launcher",ROCKET_PREFIX"sky_launcher"};
    const char* packs[]={ROCKET_PREFIX"tv_backpack",ROCKET_PREFIX"sky_backpack"};
    const char* rounds[]={ROCKET_PREFIX"tv_round",ROCKET_PREFIX"sky_round"};
    const char* damage_names[]={ROCKET_PREFIX"tv_blast",ROCKET_PREFIX"sky_blast"};
    static uint64_t payload[2][2];
    for (unsigned i=0;i<2;++i) {
        HfDamage damage={.size=sizeof(damage),.base=563,.name=damage_names[i]};
        rocket_special_damage[i]=api->add_damage(mod,&damage);
        HfProjectile projectile={.size=sizeof(projectile),.base=HF_PROJECTILE_EAT17_EXPENDABLE_ANTITANK,
            .name=rounds[i],.damage=rocket_special_damage[i]};
        rocket_special_projectiles[i]=api->add_projectile(mod,&projectile);
        if (!rocket_special_damage[i] || !rocket_special_projectiles[i]) return false;
        HfEntity weapon={.size=sizeof(weapon),.base=HF_ENTITY_FAF14_SPEAR,.name=names[i],
            .own_all=1,.title=titles[i],.description=descriptions[i]};
        rocket_special_weapons[i]=api->add_entity(mod,&weapon);
        const HfRef w=rocket_special_weapons[i];
        if (!w || hf_set(mod,w,SensorEye,range,HF_F32(600)) ||
            hf_set(mod,w,ProjectileWeapon,projectile,HF_U32(rocket_special_projectiles[i])) ||
            hf_set(mod,w,ProjectileWeapon,projectile_entity,HF_U64(0)) ||
            hf_set(mod,w,ProjectileWeapon,weapon_function_projectile_type,HF_U32(rocket_special_projectiles[i])) ||
            hf_set(mod,w,WeaponMagazine,capacity,HF_U32(i?7:3)) ||
            hf_set(mod,w,WeaponMagazine,magazine_type,HF_U32(0)) ||
            hf_set(mod,w,WeaponMagazine,first_projectile,HF_U32(rocket_special_projectiles[i])) ||
            hf_set(mod,w,WeaponMagazine,chambered,HF_U8(0)) ||
            hf_set(mod,w,WeaponMagazine,starting_magazines,HF_U32(0)) ||
            hf_set(mod,w,WeaponMagazine,max_magazines,HF_U32(0)) ||
            hf_set(mod,w,WeaponData,weapon_function_left,HF_U32(0)) ||
            hf_set(mod,w,WeaponData,weapon_function_right,HF_U32(0))) return false;
        for (unsigned n=0;n<32;++n) {
            const HfField field=HF_FIELD_AT(WeaponMagazine,pattern,n);
            const HfValue value=HF_U32(rocket_special_projectiles[i]);
            if (api->set_field(mod,w,&field,&value)) return false;
        }
        for (unsigned n=0;n<3;++n) {
            const HfField field=HF_FIELD_AT(ProjectileWeapon,fire_rate,n);
            const HfValue value=HF_F32(i?300:60);
            if (api->set_field(mod,w,&field,&value)) return false;
        }
        HfEntity backpack={.size=sizeof(backpack),.base=HF_ENTITY_BELTFED_GRENADE_LAUNCHER_BACKPACK,
            .name=packs[i],.own_all=1,.title=i?"天穹导弹弹药背包":"数控火箭弹药背包"};
        rocket_special_packs[i]=api->add_entity(mod,&backpack);
        const HfRef pack=rocket_special_packs[i];
        if (!pack || hf_set(mod,pack,Deposit,capacity,HF_U32(i?36:10)) ||
            hf_set(mod,pack,Deposit,start_amount,HF_I32(i?36:10)) ||
            hf_set(mod,pack,Deposit,refill_amount,HF_U32(i?18:5)) ||
            hf_set(mod,pack,Deposit,assisted_reload_weapon,HF_U64(w))) return false;
        HfEntity rack={.size=sizeof(rack),.base=ROCKET_RACK,
            .name=i?ROCKET_PREFIX"sky_rack":ROCKET_PREFIX"tv_rack",.own_all=1};
        const HfRef r=api->add_entity(mod,&rack);
        if (!r || hf_set(mod,r,HellpodRack,slot0_item,HF_U64(w)) ||
            hf_set(mod,r,HellpodRack,slot1_item,HF_U64(pack))) return false;
        payload[i][0]=r;payload[i][1]=ROCKET_POD;
        HfStratagem stratagem={.size=sizeof(stratagem),.base=HF_STRATAGEM_FAF14_SPEAR,
            .name=i?ROCKET_PREFIX"sky":ROCKET_PREFIX"tv",.title=titles[i],.description=descriptions[i],
            .code=i?"DLLURURUD":"DDULRUDRL",.cooldown=480,.uses=-1,
            .payload=payload[i],.payload_count=2,.package=ROCKET_SPEAR_PACKAGE,.loads_item=HF_ITEM_FAF14_SPEAR,
            .flags=HF_TRAIT_SUPPORT_WEAPON|HF_TRAIT_HELLPOD|HF_TRAIT_ANTI_TANK|HF_TRAIT_BACKPACK,
            .selection_category=heading,.selection_order=4+(int)i};
        if (ini) stratagem.cooldown=(float)GetPrivateProfileIntW(L"Stratagems",
            i?L"SkyCooldown":L"TvCooldown",480,ini);
        if (!api->add_stratagem(mod,&stratagem)) return false;
    }
    if (!rocket_tv_input_thread) rocket_tv_input_thread=CreateThread(NULL,0,rocket_tv_input_run,NULL,0,NULL);
    return true;
}
