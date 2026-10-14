#include "internal.h"

#define TOMBSTONE ((const void *)1)

static uint32_t hash_str(const char *s) {
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

static uint32_t hash_ptr(const void *p) {
    uint32_t x = (uint32_t)(uintptr_t)p;
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}

static inline uint32_t map_hash(const objc_map *m, const void *k) {
    return m->string_keys ? hash_str((const char *)k) : hash_ptr(k);
}

static inline bool map_eq(const objc_map *m, const void *a, const void *b) {
    return m->string_keys ? strcmp((const char *)a, (const char *)b) == 0 : a == b;
}

void map_init(objc_map *m, bool string_keys) {
    memset(m, 0, sizeof *m);
    m->string_keys = string_keys;
}

objc_map_entry *map_find(const objc_map *m, const void *key) {
    if (!m->capacity || !key) return NULL;
    uint32_t mask = m->capacity - 1;
    uint32_t i = map_hash(m, key) & mask;
    for (;;) {
        objc_map_entry *e = &m->entries[i];
        if (e->key == NULL) return NULL;
        if (e->key != TOMBSTONE && map_eq(m, e->key, key)) return e;
        i = (i + 1) & mask;
    }
}

void *map_get(const objc_map *m, const void *key) {
    objc_map_entry *e = map_find(m, key);
    return e ? e->value : NULL;
}

static void map_insert_fresh(objc_map *m, const void *key, void *value) {
    uint32_t mask = m->capacity - 1;
    uint32_t i = map_hash(m, key) & mask;
    for (;;) {
        objc_map_entry *e = &m->entries[i];
        if (e->key == NULL || e->key == TOMBSTONE) {
            if (e->key == NULL) m->used++;
            e->key = key; e->value = value; m->count++;
            return;
        }
        i = (i + 1) & mask;
    }
}

static void map_rehash(objc_map *m, uint32_t newcap) {
    objc_map_entry *old = m->entries;
    uint32_t oldcap = m->capacity;
    m->entries = calloc(newcap, sizeof(objc_map_entry));
    if (!m->entries) _objc_fatal("libobjc-wasm: out of memory");
    m->capacity = newcap; m->count = 0; m->used = 0;
    for (uint32_t i = 0; i < oldcap; i++) {
        if (old[i].key && old[i].key != TOMBSTONE) map_insert_fresh(m, old[i].key, old[i].value);
    }
    free(old);
}

bool map_set(objc_map *m, const void *key, void *value) {
    objc_map_entry *e = map_find(m, key);
    if (e) { e->value = value; return false; }
    if ((uint64_t)(m->used + 1) * 4 >= (uint64_t)m->capacity * 3) {
        uint32_t newcap = m->capacity ? m->capacity * 2 : 16;
        if (m->count * 2 < m->used) newcap = m->capacity;   /* mostly tombstones: compact in place */
        if (newcap < 16) newcap = 16;
        map_rehash(m, newcap);
    }
    map_insert_fresh(m, key, value);
    return true;
}

bool map_remove(objc_map *m, const void *key) {
    objc_map_entry *e = map_find(m, key);
    if (!e) return false;
    e->key = TOMBSTONE; e->value = NULL; m->count--;
    return true;
}

void map_foreach(const objc_map *m, map_visitor fn, void *ctx) {
    for (uint32_t i = 0; i < m->capacity; i++) {
        const void *k = m->entries[i].key;
        if (k && k != TOMBSTONE) fn(k, m->entries[i].value, ctx);
    }
}

void map_free(objc_map *m) {
    free(m->entries);
    m->entries = NULL; m->capacity = m->count = m->used = 0;
}

/* ---- vectors ---- */

static void vec_grow(ptr_vec *v) {
    uint32_t cap = v->capacity ? v->capacity * 2 : 4;
    void **items = realloc(v->items, cap * sizeof(void *));
    if (!items) _objc_fatal("libobjc-wasm: out of memory");
    v->items = items; v->capacity = cap;
}

void vec_push(ptr_vec *v, void *item) {
    if (v->count == v->capacity) vec_grow(v);
    v->items[v->count++] = item;
}

void vec_insert_front(ptr_vec *v, void *item) {
    if (v->count == v->capacity) vec_grow(v);
    memmove(v->items + 1, v->items, v->count * sizeof(void *));
    v->items[0] = item; v->count++;
}

bool vec_remove(ptr_vec *v, void *item) {
    for (uint32_t i = 0; i < v->count; i++) {
        if (v->items[i] == item) {
            memmove(v->items + i, v->items + i + 1, (v->count - i - 1) * sizeof(void *));
            v->count--;
            return true;
        }
    }
    return false;
}

bool vec_contains(const ptr_vec *v, const void *item) {
    for (uint32_t i = 0; i < v->count; i++) if (v->items[i] == item) return true;
    return false;
}

void vec_free(ptr_vec *v) {
    free(v->items);
    v->items = NULL; v->count = v->capacity = 0;
}
