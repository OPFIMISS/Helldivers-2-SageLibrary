#include "hellforge_mod.h"
#include "optical_wear.h"
#include "module_assets.h"

#if HF_MOD_SDK_VERSION != 16
#error This experiment requires the HellForge 0.4 SDK 16.
#endif

#define PREFIX "liuli/optical_module/"
static uint32_t module_type;
static HfRef module_resource;
static bool module_action_ready;

static void module_availability_tick(HfMod* mod)
{
    HfStratagemSettings* row = (HfStratagemSettings*)HF_API(mod)->stratagem_row(module_type);
    if (row && row->type == module_type)
        row->uses = module_pickup_ready && module_action_ready && module_solo(mod) ? UINT32_MAX : 0;
}

#include "optical_hud.h"

static const HfSet module_fields[] = {
    {HF_FIELD(Deposit, capacity), HF_U32(0)},
    {HF_FIELD(Deposit, start_amount), HF_I32(0)},
    {HF_FIELD(Deposit, refill_amount), HF_U32(0)},
    {0}
};

static void module_entity_ready(HfMod* mod, HfEvent event, void* user)
{
    (void)event;
    (void)user;
    const HfApi* api = HF_API(mod);
    uint32_t stride = 0, none = 0;
    const uintptr_t game = api->module_base("game.dll", NULL);
    const uintptr_t record = api->component_record("DepositComponentData", module_resource, &stride);
    if (!game || !record || stride != sizeof(HfDeposit) ||
        !api->code_matches(game + 0xA4385A, "8B 48 24") ||
        api->read(game + 0x3483C5C, &none, sizeof(none)) ||
        api->write_u32_if(mod, record + 0x24, 2629, none)) {
        api->log(mod, "Module disabled: inherited supply action could not be removed");
        return;
    }
    module_action_ready = true;
}

static void module_load_assets(HfMod* mod, HfEvent event, void* user)
{
    (void)event;
    (void)user;
    const HfApi* api = HF_API(mod);
    const uintptr_t game = api->module_base("game.dll", NULL);
    if (game && api->code_matches(game + 0x12FE350,
            "45 85 C0 0F 84 75 01 00 00 41 56 41 57 48 83 EC 38"))
        module_request_packages = (ModulePackageRequest)(game + 0x12FE350);
    else api->log(mod, "Module: material dependencies unavailable; reversible mesh fallback will be used");
}

HF_MOD_EXPORT int hellforge_mod_init(const HfApi* api, HfMod* mod)
{
    if (!api || api->version < 16 ||
        api->size < offsetof(HfApi, add_stratagem_category) + sizeof(api->add_stratagem_category) ||
        !api->add_stratagem_category || !api->register_entity || !api->add_stratagem ||
        !api->stratagem_row || !api->component_record || !api->write_u32_if ||
        !api->is_authority || !api->read || !api->module_base || !api->code_matches ||
        !api->hook_ex || !api->session || !api->on || !api->on_game_thread ||
        !api->log || !api->build_id || strcmp(api->build_id(), "25480438"))
        return 1;
    const HfStratagemCategory category = {
        .size = sizeof(category), .name = "support", .title = "战地支援"
    };
    const uint32_t heading = api->add_stratagem_category(mod, &category);
    const HfEntityDef device = {
        .size = sizeof(device), .name = PREFIX "device_entity",
        .base = HF_ENTITY_B1_SUPPLY_PACK, .unit = PREFIX "models/optical_pack",
        .own_all = 1, .set = module_fields,
        .title = "安装光学迷彩模块",
        .description = "拾取安装，不替换当前背包。已安装时不重复消耗装置。"
    };
    module_resource = api->register_entity(mod, &device);
    if (!module_resource) return 1;
    static HfSet rack_fields[3];
    rack_fields[0] = (HfSet){HF_FIELD(HellpodRack, slot0_item), HF_U64(module_resource)};
    rack_fields[1] = (HfSet){HF_FIELD(HellpodRack, slot1_item), HF_U64(module_resource)};
    const HfEntityDef rack = {
        .size = sizeof(rack), .name = PREFIX "device_rack",
        .base = 0x31400A6A3003E29Cull, .own_all = 1, .set = rack_fields
    };
    const HfRef rack_resource = api->register_entity(mod, &rack);
    if (!rack_resource) return 1;
    static uint64_t payload[2];
    payload[0] = rack_resource;
    payload[1] = 0x73F8498BFFDCF415ull;
    const HfStratagem stratagem = {
        .size = sizeof(stratagem), .base = HF_STRATAGEM_B1_SUPPLY_PACK,
        .name = PREFIX "optical_module", .title = "光学迷彩模块（试验）",
        .description = "单人试验：空投装置，拾取后安装为独立模块，不占背包槽。30%能量可启动迷彩；死亡后丢失。",
        .code = "DDLLRULDR", .icon = PREFIX "icons/optical",
        .cooldown = 160.0f, .uses = -1, .payload = payload, .payload_count = 2,
        .package = 0x519980A75EBD50ADull, .loads_item = HF_ITEM_B1_SUPPLY_PACK,
        .selection_category = heading
    };
    module_type = api->add_stratagem(mod, &stratagem);
    if (!module_type) return 1;
    api->on(mod, HF_EVENT_ENTITIES_READY, module_entity_ready, NULL);
    api->on(mod, HF_EVENT_GAME_LOADED, module_load_assets, NULL);
    optical_track_install(mod, module_resource, optical_gui_tick);
    api->log(mod, "Optical module experiment 0.1.1: solo only; refraction presentation, native slot userdata supported");
    return 0;
}
