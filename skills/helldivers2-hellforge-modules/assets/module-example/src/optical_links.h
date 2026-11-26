#pragma once

typedef void (*OpticalLinkUnit)(void*, void*, int32_t, void*, uint16_t);
typedef void (*OpticalUnlinkUnit)(void*, void*);
typedef struct OpticalLink { uint32_t child, parent; } OpticalLink;
static OpticalLinkUnit optical_link_original;
static OpticalUnlinkUnit optical_unlink_original;
static SRWLOCK optical_links_lock = SRWLOCK_INIT;
static OpticalLink* optical_links;
static size_t optical_links_count, optical_links_capacity;
static _Atomic uint32_t optical_visual_roots[5];
static _Atomic uint32_t optical_visual_revision;
static _Atomic uint32_t optical_equipped_unit;

static size_t optical_link_slot(uint32_t child)
{
    size_t first = 0, last = optical_links_count;
    while (first < last) {
        const size_t mid = first + (last - first) / 2;
        if (optical_links[mid].child < child) first = mid + 1;
        else last = mid;
    }
    return first;
}

/* Called under the link lock. Handles include generations, so reused slots
 * cannot inherit a destroyed character's attachment relationship. */
static bool optical_descendant(uint32_t unit, uint32_t root)
{
    for (unsigned depth = 0; unit && depth < 32; ++depth) {
        if (unit == root) return true;
        const size_t at = optical_link_slot(unit);
        if (at == optical_links_count || optical_links[at].child != unit) break;
        unit = optical_links[at].parent;
    }
    return false;
}

static bool optical_visual_hidden(uint32_t unit)
{
    uint32_t roots[5];
    bool any = false;
    for (unsigned i = 0; i < 5; ++i) {
        roots[i] = atomic_load_explicit(&optical_visual_roots[i], memory_order_relaxed);
        if (unit && unit == roots[i]) return true;
        any |= roots[i] != 0;
    }
    if (!unit || !any) return false;
    bool hidden = false;
    AcquireSRWLockShared(&optical_links_lock);
    for (unsigned i = 0; i < 5 && !hidden; ++i)
        hidden = roots[i] && optical_descendant(unit, roots[i]);
    ReleaseSRWLockShared(&optical_links_lock);
    return hidden;
}

static void optical_link_record(uint32_t child, uint32_t parent)
{
    if (!child || child == parent) return;
    AcquireSRWLockExclusive(&optical_links_lock);
    const size_t at = optical_link_slot(child);
    if (at < optical_links_count && optical_links[at].child == child) {
        if (parent) optical_links[at].parent = parent;
        else {
            memmove(optical_links + at, optical_links + at + 1,
                (--optical_links_count - at) * sizeof(*optical_links));
        }
    } else if (parent) {
        if (optical_links_count == optical_links_capacity && optical_links_capacity < 65536) {
            const size_t capacity = optical_links_capacity ? optical_links_capacity * 2 : 1024;
            OpticalLink* grown = optical_links ?
                HeapReAlloc(GetProcessHeap(), 0, optical_links, capacity * sizeof(*grown)) :
                HeapAlloc(GetProcessHeap(), 0, capacity * sizeof(*grown));
            if (grown) { optical_links = grown; optical_links_capacity = capacity; }
        }
        if (optical_links_count < optical_links_capacity) {
            memmove(optical_links + at + 1, optical_links + at,
                (optical_links_count - at) * sizeof(*optical_links));
            optical_links[at] = (OpticalLink){child, parent};
            ++optical_links_count;
        }
    }
    ReleaseSRWLockExclusive(&optical_links_lock);
}

static void optical_unit_visible(uint32_t unit, uint8_t visible, uint8_t children)
{
    /* Lua owns the reversible material effect; do not hide its parent unit. */
    optical_visibility(unit, visible, children);
}

static void optical_unit_linked(void* world, void* child, int32_t node, void* parent, uint16_t parent_node)
{
    uint32_t child_id = 0, parent_id = 0;
    if (child) memcpy(&child_id, (const uint8_t*)child + 8, 4);
    if (parent) memcpy(&parent_id, (const uint8_t*)parent + 8, 4);
    const bool was_hidden = optical_visual_hidden(child_id);
    optical_link_original(world, child, node, parent, parent_node);
    optical_link_record(child_id, parent_id);
    const bool hidden = optical_visual_hidden(child_id);
    if (hidden || was_hidden) {
        atomic_fetch_add_explicit(&optical_visual_revision, 1, memory_order_relaxed);
    }
}

static void optical_unit_unlinked(void* world, void* unit)
{
    uint32_t id = 0;
    if (unit) memcpy(&id, (const uint8_t*)unit + 8, 4);
    const bool was_hidden = optical_visual_hidden(id);
    optical_unlink_original(world, unit);
    optical_link_record(id, 0);
    if (was_hidden) {
        atomic_fetch_add_explicit(&optical_visual_revision, 1, memory_order_relaxed);
    }
}
