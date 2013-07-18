#include "map.h"
#include <string.h>
#include <assert.h>

struct _element {
    const char* key;
    size_t hash;
    void* pointer;
    size_t next;
};

struct map {
    size_t size;
    size_t cap;
    struct _element* elems;
};

struct map*
map_new(size_t max) {
    struct map* m = malloc(sizeof(struct map));
    size_t cap = 1;
    while (cap < max) {
        cap *= 2;
    }
    m->cap = cap;
    m->size = 0;
    m->elems = malloc(sizeof(struct _element) * cap);
    memset(m->elems, 0, sizeof(struct _element) * cap);
    return m;
}

void
map_free(struct map* self) {
    if (self) {
        free(self->elems);
        free(self);
    }
}

size_t
_calc_hash(const char* key) {
    size_t len = strlen(key);
    size_t h = len;
    size_t step = (len>>5)+1;
    size_t i;
    for (i=len; i>=step; i-=step)
        h = h ^ ((h<<5)+(h>>2)+(size_t)key[i-1]);
    return h;   
}

void*
map_find(struct map* self, const char* key) {
    size_t hash = _calc_hash(key);
    size_t index = hash & (self->cap - 1);
    struct _element* elem = &self->elems[index];
    while (elem->key) {
        if (elem->hash == hash && strcmp(elem->key, key) == 0)
            return elem->pointer;
        if (elem->next <= 0)
            return NULL;
        assert(elem->next > 0 && elem->next <= self->cap);
        elem = &self->elems[elem->next-1];
    }
    return NULL;
}

// has enough cap
static void
_insert(struct map* self, const char* key, size_t hash, void* pointer) { 
    size_t hash_index = hash & (self->cap - 1);
    struct _element* elem = &self->elems[hash_index];
    if (elem->key == NULL) {
        elem->key = key;
        elem->hash = hash;
        elem->pointer = pointer;
    } else {
        struct _element* next = NULL;
        size_t next_index = hash_index + 1;
        int i;
        for (i=0; i<self->cap; ++i) {
            next_index = (next_index+i) & (self->cap-1);
            next = &self->elems[next_index];
            if (next->key == NULL)
                break;
        }
        assert(next && next->key == NULL);
        next->key = key;
        next->hash = hash;
        next->pointer = pointer;
        next->next = elem->next;
        elem->next = next_index + 1;
    }
    self->size += 1;
}

void
_rehash(struct map* self) {
    struct _element* old_elems = self->elems;
    size_t old_cap = self->cap;
   
    self->size = 0;
    self->cap *= 2;
    self->elems = malloc(sizeof(struct _element) * self->cap);
    memset(self->elems, 0, sizeof(struct _element) * self->cap); 

    size_t i;
    for (i=0; i<old_cap; ++i) {
        struct _element* elem = &old_elems[i];
        _insert(self, elem->key, elem->hash, elem->pointer);
    }
    free(old_elems);
}

void
map_insert(struct map* self, const char* key, void* pointer) {
    if (self->size >= self->cap) {
        _rehash(self);
    }
    size_t hash = _calc_hash(key);
    _insert(self, key, hash, pointer);
}

void 
map_foreach(struct map* self, 
        void (*cb)(const char* key, void* value, void* ud), void* ud) {
    int i;
    for (i=0; i<self->cap; ++i) {
        if (self->elems[i].key) {
            cb(self->elems[i].key, self->elems[i].pointer, ud);
        }
    }
}

int
map_foreach_check(struct map* self, 
        int (*cb)(const char* key, void* value, void* ud), void* ud) {
    int i;
    for (i=0; i<self->cap; ++i) {
        if (self->elems[i].key) {
            if (cb(self->elems[i].key, self->elems[i].pointer, ud))
                return 1;
        }
    }
    return 0;
}
