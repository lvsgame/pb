#ifndef __MAP_H__
#define __MAP_H__

#include <stdlib.h>

struct map;

struct map* map_new(size_t max);
void map_free(struct map* self);
void* map_find(struct map* self, const char* key);
void map_insert(struct map* self, const char* key, void* pointer);

void map_foreach(struct map* self, 
        void (*cb)(const char* key, void* value, void* ud), void* ud);
int  map_foreach_check(struct map* self, 
        int (*cb)(const char* key, void* value, void* ud), void* ud);

#endif
