#pragma once

/* Body entities from the build-25480438 catalogue. Never classify by hit points:
 * a wounded heavy stays heavy, and a buffed scavenger never becomes lockable. */
typedef struct RocketTarget {
    uint32_t entity, unit;
    uint32_t node;
    uint8_t rank;
    float position[3];
} RocketTarget;
static RocketTarget rocket_targets[2048];
static unsigned rocket_target_count;
static HfRef rocket_named_small[12];
static struct { uint32_t unit; float offset[3]; } rocket_weak_cache[64];
static unsigned rocket_weak_cursor;
static bool rocket_native_point(const HfApi*,uint32_t,uint32_t,float[3]);
static bool rocket_native_node(const HfApi*,uint32_t,uint32_t*,float[3]);

static void rocket_cache_weakpoint(uint32_t unit,const float root[3],const float point[3])
{
    unsigned at=64;
    for (unsigned i=0;i<64;++i) if (rocket_weak_cache[i].unit==unit) {at=i;break;}
    if (at==64) at=rocket_weak_cursor++%64;
    rocket_weak_cache[at].unit=unit;
    for (unsigned j=0;j<3;++j) rocket_weak_cache[at].offset[j]=point[j]-root[j];
}

static unsigned rocket_target_rank(uint64_t resource)
{
    static const struct { uint64_t resource; unsigned rank; } profiles[] = {
        /* Destructible enemy structures from the local entity catalogue. */
        {0x4232EE48E2CFD24Eull,1},{0xA8AE6952B375EF6Cull,1},
        {0x095686275A113614ull,1},{0xDC901B71A3A73B9Aull,1},
        {0xF0B98FB953B13960ull,1},{0x5158E582FBEB26BDull,1},
        {0x22CC0ED4CEB9CB68ull,1},{0xA1A7B76B29088843ull,1},
        {0x4FB8EB356AC8553Eull,1},{0x6E499C5C95B019FCull,1},
        {0x298C464F7F4261E3ull,1},{0x485C04E03CCC969Eull,1},
        {0xE8E7D998FCBD32F8ull,1},{0xB3C9CDB79DC17937ull,1},
        {0x0883366204E1CCC5ull,2},{0x1448D494665D01A0ull,2},
        {0x0AB7B92B131C228Cull,2},{0x137988CEA16458F7ull,2},
        {0x1897BDD32105D2DCull,2},{0x1A7FCDFF98C664B0ull,2},
        {0x1E66EE1F6F7FD00Eull,2},{0x282EB766C1FFA6A1ull,2},
        {0x30F2DEE2333F227Aull,2},{0x31BAE74D2F064D8Dull,2},
        {0x32CDEADA234FB8DFull,2},{0x3AFF5FD7D5450B99ull,2},
        {0x3E0537D606438FEAull,2},{0x53D8919D7B8ABD67ull,2},
        {0x63DF3D07B7424588ull,2},{0x67DC32DCA4F02D33ull,2},
        {0x6B202392F4AB605Eull,2},{0x6DAB2EADF5D8B692ull,2},
        {0x8FC2784308E9BA99ull,2},{0x965EAE5A51ACDD4Aull,2},
        {0x96BA14C9EBB49CE1ull,2},{0x9E2E17F2CCCCAFDDull,2},
        {0xA05BD1EC67B3AC4Cull,2},{0xB971B1141E1CBFACull,2},
        {0xBC242702FB46B7E7ull,2},{0xC2B9EB21B06F8378ull,2},
        {0xC6449FFD9EA3779Cull,2},{0xD37E8D120D2836E3ull,2},
        {0xD63FCBFF0851B7AFull,2},{0xD84FFC32A6E640A8ull,2},
        {0xDCF8E74212FBEE3Bull,2},{0xE44EC9F9B3FE1D2Aull,2},
        {0xEF04CB84D097A497ull,2},{0xEF570293245A17C2ull,2},
        {0xF22D027B37BEF107ull,2},{HF_ENTITY_HIVE_LORD,2},
        {0x08E6FFC2474287BDull,1},{0x10081ACEF6163EF6ull,1},
        {0x20B9C7734DAEAD65ull,1},{0x32541FC4EC7C9CDCull,1},
        {0x34DFD23365472E9Eull,1},{0x36AA99CCE5E60146ull,1},
        {0x54E107DACF6929CBull,1},{0x57EED0EAC346CD9Dull,1},
        {0x58B2B86C11369241ull,1},{0x6021E22338333D88ull,1},
        {0x604A794EC45BB820ull,1},{0x728421351D440EBCull,1},
        {0x9076EEED17FCEE35ull,1},{0x91EBD77931110AFCull,1},
        {0xA1F37BF2A40FBDE4ull,1},{0xA35207C6F2150806ull,1},
        {0xA381A11C07D3EB94ull,1},{0xA6A68D8AF177F3A1ull,1},
        {0xAE63E525853D7044ull,1},{0xB2A6FA1E4284C7E6ull,1},
        {0xB5DBC0C240C921ADull,1},{0xB92435FBF60F0748ull,1},
        {0xBE39E313A1E46BB9ull,1},{0xBE743B2FAA3A6E26ull,1},
        {0xC626D2BB495A202Dull,1},{0xCC7022FDD172089Bull,1},
        {0xCCAE5264ACD591B7ull,1},{0xD1E990BAF22D5A52ull,1},
        {0xD522FD4748D443A5ull,1},{0xD5792F6856B06BA4ull,1},
        {0xDA40BB347C7447F2ull,1},{0xE0353177F1329573ull,1},
        {0xE8F19A0AA958E46Dull,1},{0xEACEE39FA017B495ull,1},
        {0xF1610AC48CDC5240ull,1},{0xF540CA9D9D4A422Eull,1},
        {0xF79CD8BB654397DFull,1},{0xF8131632AA867107ull,1},
        {0xF8B5A81A86D5D4EBull,1},{0xFC8DEC78BE8AB47Dull,1}
    };
    for (unsigned i=0; i<sizeof(profiles)/sizeof(*profiles); ++i)
        if (resource==profiles[i].resource) return profiles[i].rank;
    return 0;
}

static bool rocket_small_threat(uint64_t resource)
{
    static const HfRef bodies[]={HF_ENTITY_ASSAULT_RAIDER,HF_ENTITY_BILE_SPITTER,
        HF_ENTITY_BRAWLER,HF_ENTITY_MARAUDER,HF_ENTITY_POUNCER,HF_ENTITY_PREDATOR_HUNTER,
        HF_ENTITY_PYRO_TROOPER,HF_ENTITY_ROCKET_RAIDER,HF_ENTITY_SCOUT_RAIDER,
        HF_ENTITY_SPORE_BURST_HUNTER,HF_ENTITY_SPORE_BURST_SCAVENGER,
        HF_ENTITY_WATCHER,HF_ENTITY_WRETCH};
    for (unsigned i=0;i<sizeof(bodies)/sizeof(*bodies);++i)
        if (resource==bodies[i]) return true;
    for (unsigned i=0;i<sizeof(rocket_named_small)/sizeof(*rocket_named_small);++i)
        if (rocket_named_small[i] && resource==rocket_named_small[i]) return true;
    return false;
}

static void rocket_target_names(const HfApi* api)
{
    /* Original resource paths from the local catalogue. These unnamed variants
     * are proximity triggers only; no inferred faction or health threshold. */
    static const char* names[]={
        "content/fac_bugs/cha_hunter/cha_hunter_base",
        "content/fac_bugs/cha_hunter/cha_hunter_tier_1",
        "content/fac_bugs/cha_hunter/cha_hunter_tier_2",
        "content/fac_bugs/cha_hunter/cha_hunter_tier_3",
        "content/fac_bugs/cha_hunter/cha_hunter_tier3",
        "content/fac_bugs/cha_hunter/cha_hunter_gloom",
        "content/fac_bugs/cha_hunter/cha_hunter_gloom_tier1",
        "content/fac_bugs/cha_scavenger/cha_scavenger_base",
        "content/fac_bugs/cha_scavenger/cha_scavenger_tier_1",
        "content/fac_bugs/cha_scavenger/cha_scavenger_tier_2",
        "content/fac_bugs/cha_scavenger/cha_scavenger_gloom",
        "content/fac_bugs/cha_scavenger_predator/cha_scavenger_predator"
    };
    for (unsigned i=0;i<sizeof(names)/sizeof(*names);++i)
        rocket_named_small[i]=api->resource_hash(names[i]);
}

static float rocket_distance2(const float a[3], const float b[3])
{
    float d=0;
    for (unsigned i=0;i<3;++i) { const float v=a[i]-b[i]; d+=v*v; }
    return d;
}

static bool rocket_normalize(float v[3])
{
    const float len=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if (!isfinite(len) || len<0.0001f) return false;
    for (unsigned i=0;i<3;++i) v[i]/=len;
    return true;
}

static RocketTarget* rocket_find_target(uint32_t entity)
{
    for (unsigned i=0;i<rocket_target_count;++i)
        if (rocket_targets[i].entity==entity) return &rocket_targets[i];
    return NULL;
}

static void rocket_scan_targets(HfMod* mod, const float origin[3])
{
    const HfApi* api=HF_API(mod);
    const HfHealthManager* h=hf_global_HealthManager(mod);
    rocket_target_count=0;
    if (!h || h->count>65536 || !h->descriptors || !h->records) return;
    const uint32_t count=h->count;
    const uintptr_t descriptors=(uintptr_t)h->descriptors, records=(uintptr_t)h->records;
    for (unsigned i=0;i<count && rocket_target_count<2048;++i) {
        const uintptr_t d=optical_ptr(api,descriptors+i*8);
        struct { uint64_t resource; uint32_t entity,unit,network,flags; } body;
        uint32_t life=2;
        int32_t hp=0;
        if (!d || api->read(d,&body,sizeof(body))) continue;
        if (api->read(records+i*0x1B8+0x14,&hp,4) || hp<=0 ||
            api->read(records+i*0x1B8+0x19C,&life,4) || life!=0) continue;
        unsigned rank=rocket_target_rank(body.resource);
        uint32_t node=16;
        float native_point[3];
        const bool small=rocket_small_threat(body.resource);
        const bool native_target=!small && rocket_native_node(api,body.entity,&node,native_point);
        if (!rank && !native_target && !small) continue;
        if (native_target && !rank) rank=1;
        RocketTarget candidate={.entity=body.entity,.unit=body.unit,.node=node,.rank=(uint8_t)rank};
        if (!optical_unit_position(api,body.unit,candidate.position)) continue;
        if (native_target) memcpy(candidate.position,native_point,sizeof(native_point));
        rocket_targets[rocket_target_count++]=candidate;
    }
    (void)origin; /* Each shooter/rocket applies its own 600 m range below. */
}

static RocketTarget* rocket_acquire(const float origin[3], const float forward[3])
{
    RocketTarget* best=NULL;
    float best_score=1e30f;
    for (unsigned i=0;i<rocket_target_count;++i) {
        RocketTarget* t=&rocket_targets[i];
        if (!t->rank) continue;
        float dir[3];
        for (unsigned j=0;j<3;++j) dir[j]=t->position[j]-origin[j];
        const float distance=sqrtf(rocket_distance2(origin,t->position));
        if (distance>600 || !rocket_normalize(dir)) continue;
        const float dot=dir[0]*forward[0]+dir[1]*forward[1]+dir[2]*forward[2];
        if (dot<0.98480775f) continue; /* 10-degree acquisition cone */
        const float score=(1-dot)*1000000.0f+distance+(2-t->rank)*2.0f;
        if (score<best_score) { best_score=score; best=t; }
    }
    return best;
}

static RocketTarget* rocket_retarget(const float origin[3], uint32_t seed)
{
    RocketTarget* nearest[4]={0};
    float distances[4]={1e30f,1e30f,1e30f,1e30f};
    unsigned n=0;
    for (unsigned i=0;i<rocket_target_count;++i) {
        if (!rocket_targets[i].rank) continue;
        const float d=rocket_distance2(origin,rocket_targets[i].position);
        if (d>600.0f*600.0f) continue;
        for (unsigned k=0;k<4;++k) if (d<distances[k]) {
            for (unsigned q=3;q>k;--q) { distances[q]=distances[q-1]; nearest[q]=nearest[q-1]; }
            distances[k]=d; nearest[k]=&rocket_targets[i];
            if (n<4) ++n;
            break;
        }
    }
    return n ? nearest[seed%n] : NULL;
}

static RocketTarget* rocket_salvo_target(const float origin[3],const float forward[3],
    uint32_t primary,const uint32_t assigned[4],unsigned count)
{
    RocketTarget* centre=rocket_find_target(primary),*best=NULL;
    float score=1e30f;
    if (!count && centre) return centre;
    for (unsigned i=0;i<rocket_target_count;++i) {
        RocketTarget* t=&rocket_targets[i];
        bool used=false;
        for (unsigned j=0;j<count;++j) used|=assigned[j]==t->entity;
        if (used) continue;
        float direction[3];
        for (unsigned j=0;j<3;++j) direction[j]=t->position[j]-origin[j];
        const float d=rocket_distance2(origin,t->position);
        if (d>600.0f*600.0f || !rocket_normalize(direction) ||
            direction[0]*forward[0]+direction[1]*forward[1]+direction[2]*forward[2]<0.5f) continue;
        const float cluster=centre ? rocket_distance2(centre->position,t->position) : d;
        if (cluster>120.0f*120.0f && centre) continue;
        const float s=cluster+(2-t->rank)*1000.0f;
        if (s<score) {score=s;best=t;}
    }
    /* Reuse only when there are fewer living targets than rockets. */
    return best ? best : centre;
}

static RocketTarget* rocket_near_threat(const float position[3],const float velocity[3],float dt)
{
    const float lookahead=fminf(fmaxf(dt,0.0f),0.04f);
    float travel[3];
    for (unsigned j=0;j<3;++j) travel[j]=velocity[j]*lookahead;
    const float length2=travel[0]*travel[0]+travel[1]*travel[1]+travel[2]*travel[2];
    for (unsigned i=0;i<rocket_target_count;++i) {
        const float* p=rocket_targets[i].position;
        float along=0;
        for (unsigned j=0;j<3;++j) along+=(p[j]-position[j])*travel[j];
        along=length2>0.001f ? fminf(1.0f,fmaxf(0.0f,along/length2)) : 0;
        float distance2=0;
        for (unsigned j=0;j<3;++j) {
            const float delta=p[j]-(position[j]+travel[j]*along);
            distance2+=delta*delta;
        }
        if (distance2<=144.0f) return &rocket_targets[i];
    }
    return NULL;
}
