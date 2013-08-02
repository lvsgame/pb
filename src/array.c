#include "array.h"
#include <string.h>

struct array {
    size_t size;
    size_t cap;
    void** elems;
};

struct array* 
array_new(size_t max) {
    struct array* a = malloc(sizeof(struct array));
    size_t cap = 1;
    while (cap < max) {
        cap *= 2;
    }
    a->cap = cap;
    a->size = 0;
    a->elems = malloc(sizeof(void*) * cap);
    memset(a->elems, 0, sizeof(void*) * cap);
    return a;
}

void 
array_free(struct array* self) {
    if (self) {
        free(self->elems);
        free(self);
    }
}

inline static void
_grow(struct array* self, size_t cap) {
    size_t old_cap = self->cap;
    while (self->cap < cap)
        self->cap *= 2;
    self->elems = realloc(self->elems, sizeof(void*) * self->cap);
    memset(self->elems + old_cap,  0, sizeof(void*) * (self->cap - old_cap));
}

void 
array_set(struct array* self, size_t index, void* pointer) {
    if (index >= self->cap) {
        _grow(self, index+1);
    }
    self->elems[index] = pointer;
    if (index >= self->size) {
        self->size = index + 1;
    }
}

void*
array_get(struct array* self, size_t index) {
    if (index >= self->cap) {
        return NULL;
    }
    return self->elems[index];
}

size_t 
array_size(struct array* self) {
    return self->size;
}

size_t 
array_capacity(struct array* self) {
    return self->cap;
}

void array_foreach(struct array* self, 
        void (*cb)(size_t index, void* p, void* ud), void* ud) {
    size_t i;
    for (i=0; i<self->size; ++i) {
        if (self->elems[i]) {
            cb(i, self->elems[i], ud);
        }
    }
}
