#ifndef __ARRAY_H__
#define __ARRAY_H__

#include <stdlib.h>

struct array;

struct array* array_new(size_t max);
void array_free(struct array* self);

void array_set(struct array* self, size_t index, void* pointer);
void* array_get(struct array* self, size_t index); 

size_t array_size(struct array* self);
size_t array_capacity(struct array* self);

void array_foreach(struct array* self, 
        void (*cb)(size_t index, void* p, void* ud), void* ud);

#endif
