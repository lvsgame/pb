#ifndef __PB_H__
#define __PB_H__

#include <stdlib.h>
#include <stdint.h>

struct pb_slice {
    size_t size;
    size_t cur;
    char* pointer;
};

struct pb_field_decl {
    const char* name;
    int number;
    int offset;
    int bytes;
    uint16_t repeat_max;
    int repeat_offset;
    const char* type;
};

struct pb_context;

struct pb_context* pb_context_new(size_t object_max);
void pb_context_delete(struct pb_context* pbc);
int pb_context_object(struct pb_context* pbc, const char* name, 
        struct pb_field_decl* fds, size_t n);
void pb_context_fresh(struct pb_context* pbc);
int pb_context_verify(struct pb_context* pbc);
int pb_context_seri(struct pb_context* pbc, const char* name, 
        struct pb_slice* c, struct pb_slice* seri);
int pb_context_unseri(struct pb_context* pbc, const char* name,
        struct pb_slice* seri, struct pb_slice* c);
const char* pb_context_lasterror(struct pb_context* pbc);
void pb_context_dump(struct pb_context* pbc);


#endif
