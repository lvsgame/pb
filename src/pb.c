#include "pb.h"
#include "array.h"
#include "map.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define LABEL_REQUIRED 0
#define LABEL_OPTIONAL 1
#define LABEL_REPEATED 2

#define WT_VARINT 0
#define WT_BIT64  1
#define WT_LEND   2
#define WT_BIT32  5

#define CT_BOOL     0
#define CT_UINT8    1
#define CT_UINT16   2
#define CT_UINT32   3
#define CT_UINT64   4
#define CT_INT8     5
#define CT_INT16    6
#define CT_INT32    7
#define CT_INT64    8

#define CT_FIXED32  9
#define CT_FIXED64  10
#define CT_SFIXED32 11
#define CT_SFIXED64 12
#define CT_FLOAT    13
#define CT_DOUBLE   14

#define CT_STRING   15
#define CT_BYTES    16
#define CT_OBJECT   17

#define FIELD_NUMBER_MAX  ((1<<12)-1)

#define PB_OK 0
#define PB_ERR_NO_WBUFFER 1
#define PB_ERR_NO_RBUFFER 2
#define PB_ERR_NO_CTYPE   3  
#define PB_ERR_NO_WTYPE   4
#define PB_ERR_LABEL      5
#define PB_ERR_REPEAT     6
#define PB_ERR_NUMBER     7
#define PB_ERR_VARINT     8
#define PB_ERR_VERIFY_WTYPE    9
#define PB_ERR_VERIFY_NUMBER   10
#define PB_ERR_OBJECT_TOODEPTH 11

static const char* 
_str_error(int error) {
    static const char* _ERRORS[] = {
        "ok",
        "write buffer no enough",
        "read buffer no enough",
        "ctype error",
        "wtype error",
        "label error",
        "repeat error",
        "number error",
        "varint error",
        "wtype verify fail",
        "number verify fail",
        "object too depth",
    };
    if (error >= 0 &&
        error < sizeof(_ERRORS)/sizeof(_ERRORS[0]))
        return _ERRORS[error];
    return "";
};

struct _field {
    char* name;
    int number;
    int offset;
    int bytes;
    uint16_t repeat_max;
    int repeat_offset;
    char* tname;
    int ctype;
    int wtype;
    int label;
    struct _object* child;
};

struct _object {
    const char* name;
    struct array* fields;
};

inline static size_t
_varint_encode(uint64_t var, char buffer[8]) {
    size_t i;
    for (i=0; i<8; ++i) {
        buffer[i] = (uint8_t)(var&0x7f);
        var >>= 7;
        if (var)
            buffer[i] |= 0x80;
        else
            break;
    }
    return i+1;
}

inline static size_t 
_varint_decode(char* buffer, size_t n, uint64_t* var) {
    if (n > 8) n = 8;
    *var = 0;
    size_t i;
    for (i=0; i<n; ++i) {
        *var |= ((uint64_t)buffer[i]&0x7f) << (i*7);
        if ((buffer[i] & 0x80) == 0)
            break;
    }
    return i+1;
}

inline static int
_slice_write(struct pb_slice* s, void* p, int size) {
    if (s->cur + size > s->size)
        return 1;
    memcpy(s->pointer + s->cur, p, size);
    return 0;
}

inline static int
_slice_checkforward(struct pb_slice* s, int size) {
    return s->cur + size <= s->size ? 0 : 1;
}

static inline int
_pack_tag(struct _field* f, struct pb_slice* seri) {
    size_t n = 0;
    char buffer[2];
    if (f->number < 0) {
        return PB_ERR_NUMBER;
    } else if (f->number < 16) {
        buffer[0] = (f->number<<3) | f->wtype;
        n = 1;
    } else if (f->number < 4096) {
        buffer[0] = (f->number<<3) | f->wtype | 0x80;
        buffer[1] = (f->number>>4);
        n = 2;
    } else {
        return PB_ERR_NUMBER;
    }
    if (_slice_write(seri, &buffer, n))
        return PB_ERR_NO_WBUFFER;
    seri->cur += n;
    return PB_OK;
}

static inline int
_unpack_tag(struct pb_slice* seri, int* number, int* wtype) {
    if (_slice_checkforward(seri, 1))
        return PB_ERR_NO_RBUFFER;
    char* buffer = seri->pointer+seri->cur;
    *wtype = buffer[0] & 0x07;
    *number = (buffer[0] & 0x78) >> 3;
    if (buffer[0] & 0x80) {
        if (_slice_checkforward(seri, 1))
            return PB_ERR_NO_RBUFFER;
        *number |= buffer[1];
        seri->cur += 2;
    } else
        seri->cur += 1;
    return PB_OK;
}

inline static int
_pack_varint(struct _field* f, struct pb_slice* c, struct pb_slice* seri) {
    assert(f->bytes <= 8);
    if (_slice_checkforward(c, f->bytes))
        return PB_ERR_NO_RBUFFER;
    uint64_t var = 0;
    memcpy(&var, c->pointer+c->cur, f->bytes);
    switch (f->ctype) {
    case CT_INT32:
        var = ((int32_t)var<<1) ^ ((int32_t)var>>31);
        break;
    case CT_INT64:
        var = ((int64_t)var<<1) ^ ((int64_t)var>>63);
        break;
    default:
        break;
    }
    char buffer[8];
    size_t n = _varint_encode(var, buffer);
    if (_slice_write(seri, buffer, n))
        return PB_ERR_NO_WBUFFER;
    seri->cur += n;
    return PB_OK;
}

inline static int
_unpack_varint(struct _field* f, struct pb_slice* seri, struct pb_slice* c) {
    assert(f->bytes <= 8);
    size_t n = seri->size - seri->cur;
    if (n == 0)
        return PB_ERR_NO_RBUFFER;
    char* buffer = seri->pointer + seri->cur;
    uint64_t var;
    n = _varint_decode(buffer, n, &var);
    if (n > f->bytes)
        return PB_ERR_VARINT;
    switch (f->ctype) {
    case CT_INT32:
    case CT_INT64:
        var = (var&1) ? ~(var>>1) : var>>1;
        break;
    default:
        break;
    }
    if (_slice_write(c, &var, f->bytes))
        return PB_ERR_NO_WBUFFER;
    seri->cur += n;
    return PB_OK;
}

inline static int
_pack_bytes(size_t bytes, struct pb_slice* c, struct pb_slice* seri) {
    if (_slice_checkforward(c, bytes))
        return PB_ERR_NO_RBUFFER;
    if (_slice_write(seri, c->pointer + c->cur, bytes))
        return PB_ERR_NO_WBUFFER;
    seri->cur += bytes;
    return PB_OK;
}

inline static int
_unpack_bytes(size_t bytes, struct pb_slice* seri, struct pb_slice* c) {
    if (_slice_checkforward(seri, bytes))
        return PB_ERR_NO_RBUFFER;
    if (_slice_write(c, seri->pointer+seri->cur, bytes))
        return PB_ERR_NO_WBUFFER;
    seri->cur += bytes;
    return PB_OK;
}

#define CHECK(f) r = f; if (r) return r;

inline static int
_pack_object(struct _object* o, struct pb_slice* c, struct pb_slice* seri);

inline static int
_pack_lend(struct _field* f, struct pb_slice* c, struct pb_slice* seri) {
    uint16_t repeat;
    if (f->repeat_offset > 0) {
        int offset = c->cur - f->repeat_offset;
        assert(f->repeat_offset >= 0);
        repeat = *(uint16_t*)(c->pointer + offset);
    } else {
        repeat = f->repeat_max;
    }

    if (_slice_checkforward(seri, 2)) {
        return PB_ERR_NO_WBUFFER;
    }

    uint16_t* bytesptr = (uint16_t*)(seri->pointer+seri->cur);
    seri->cur += 2;

    int begin = seri->cur;
    int r;
    size_t i;
    switch (f->ctype) {
    case CT_BYTES:
    case CT_STRING:
        assert(f->bytes == 1);
        CHECK(_pack_bytes(repeat, c, seri));
        c->cur += repeat;
        break;
    case CT_BOOL:
    case CT_UINT8:
    case CT_UINT16:
    case CT_UINT32:
    case CT_UINT64:
    case CT_INT8:
    case CT_INT16:
    case CT_INT32:
    case CT_INT64:
        for (i=0; i<repeat; ++i) {
            CHECK(_pack_varint(f, c, seri));
            c->cur += f->bytes;
        }
        break;
    case CT_FLOAT:
        assert(f->bytes == 4);
        for (i=0; i<repeat; ++i) {
            CHECK(_pack_bytes(f->bytes, c, seri));
            c->cur += f->bytes;
        }
        break;
    case CT_DOUBLE:
        assert(f->bytes == 8);
        for (i=0; i<repeat; ++i) {
            CHECK(_pack_bytes(f->bytes, c, seri));
            c->cur += f->bytes;
        }
        break;
    case CT_OBJECT:
        for (i=0; i<repeat; ++i) {
            CHECK(_pack_object(f->child, c, seri));
            c->cur += f->bytes;
        }
        break;
    default:
        return PB_ERR_NO_CTYPE;
    }

    *bytesptr = seri->cur-begin;
    return PB_OK;
}

inline static int
_pack_field(struct _field* f, struct pb_slice* c, struct pb_slice* seri) {
    int r;
    CHECK(_pack_tag(f, seri));
    
    int parent = c->cur;
    c->cur += f->offset;
 
    switch (f->wtype) {
    case WT_VARINT:
        CHECK(_pack_varint(f, c, seri));
        break;
    case WT_BIT64:
        assert(f->bytes == 8);
        CHECK(_pack_bytes(f->bytes, c, seri));
        break;
    case WT_BIT32: 
        assert(f->bytes == 4);
        CHECK(_pack_bytes(f->bytes, c, seri));
        break;
    case WT_LEND:
        CHECK(_pack_lend(f, c, seri));
        break;
    default:
        return PB_ERR_NO_WTYPE;
    }
    
    c->cur = parent;
    return PB_OK;
}

inline static int
_pack_object(struct _object* o, struct pb_slice* c, struct pb_slice* seri) {
    int r;
    size_t i;
    for (i=0; i<array_size(o->fields); ++i) {
        struct _field* f = array_get(o->fields, i);
        if (f) {
            CHECK(_pack_field(f, c, seri));
        }
    }
    return PB_OK;
}

inline static int
_unpack_object(struct _object* o, struct pb_slice* seri, struct pb_slice* c);

inline static int
_unpack_lend(struct _field* f, struct pb_slice* seri, struct pb_slice* c) {
    int r;
    if (_slice_checkforward(seri, 2))
        return PB_ERR_NO_RBUFFER;

    uint16_t* repeatptr = (uint16_t*)(c->pointer + c->cur - f->repeat_offset);
    uint16_t bytes = *(uint16_t*)(seri->pointer + seri->cur);
    seri->cur += 2;
    if (bytes + seri->cur > seri->size)
        return PB_ERR_NO_RBUFFER;
    
    size_t end = seri->cur + bytes;
    size_t repeat = 0;

    switch (f->ctype) {
    case CT_BYTES:
        assert(f->bytes == 1);
        CHECK(_unpack_bytes(bytes, seri, c));
        c->cur += bytes;
        repeat = bytes;
        break;
    case CT_STRING:
        assert(f->bytes == 1);
        CHECK(_unpack_bytes(bytes, seri, c));
        c->cur += bytes;
        c->pointer[c->cur-1] = '\0';
        repeat = bytes;
        break;
    case CT_BOOL:
    case CT_UINT8:
    case CT_UINT16:
    case CT_UINT32:
    case CT_UINT64:
    case CT_INT8:
    case CT_INT16:
    case CT_INT32:
    case CT_INT64:
        while (seri->cur < end) {
            CHECK(_unpack_varint(f, seri, c));
            c->cur += f->bytes;
            repeat += 1;
        } 
        break;
    case CT_FLOAT:
        assert(f->bytes == 4);
        assert(bytes % 4 == 0);
        CHECK(_unpack_bytes(bytes, seri, c));
        c->cur += bytes;
        repeat = bytes/4;
        break;
    case CT_DOUBLE:
        assert(f->bytes == 8);
        assert(bytes % 8 == 0);
        CHECK(_unpack_bytes(bytes, seri, c));
        c->cur += bytes;
        repeat = bytes/8;
        break;
    case CT_OBJECT: {
        size_t bak_size = seri->size;
        seri->size = end;
        while (seri->cur < end) {
            CHECK(_unpack_object(f->child, seri, c)); 
            c->cur += f->bytes;
            repeat += 1;
        }
        seri->size = bak_size;
        break;
    }
    default:
        return PB_ERR_NO_CTYPE;
    }
    if (f->repeat_offset > 0) {
        if (repeat > f->repeat_max)
            return PB_ERR_REPEAT;
        *repeatptr = repeat;
    } else {
        if (repeat != f->repeat_max)
            return PB_ERR_REPEAT;
    }
    return PB_OK;
}

inline static int
_unpack_field(struct _field* f, struct pb_slice* seri, struct pb_slice* c) {
    int r;
    int parent = c->cur;
    c->cur += f->offset;

    switch (f->wtype) {
    case WT_VARINT:
        CHECK(_unpack_varint(f, seri, c));
        break;
    case WT_BIT64:
        assert(f->bytes == 8);
        CHECK(_unpack_bytes(f->bytes, seri, c));
        break;
    case WT_BIT32:
        assert(f->bytes == 4);
        CHECK(_unpack_bytes(f->bytes, seri, c));
        break;
    case WT_LEND:
        CHECK(_unpack_lend(f, seri, c));
        break;
    default:
        return PB_ERR_NO_WTYPE;
    }
    
    c->cur = parent;
    return PB_OK;
}

inline static int
_skip_field(int wtype, struct pb_slice* seri) {
    switch (wtype) {
    case WT_VARINT: {
        size_t n = seri->size - seri->cur;
        if (n == 0)
            return PB_ERR_NO_RBUFFER;
        char* buffer = seri->pointer + seri->cur;
        uint64_t var;
        n = _varint_decode(buffer, n, &var);
        if (n > 8)
            return PB_ERR_VARINT;
        seri->cur += n;
        return PB_OK;
    }
    case WT_BIT64:
        seri->cur += 8;
    case WT_BIT32:
        seri->cur += 4;
    case WT_LEND: {
        if (_slice_checkforward(seri, 2))
            return PB_ERR_NO_RBUFFER;
        uint16_t bytes = *(uint16_t*)(seri->pointer+seri->cur);
        seri->cur += 2;
        if (bytes + seri->cur > seri->size)
            return PB_ERR_NO_RBUFFER;
        seri->cur += bytes;
    }
    default:
        return PB_ERR_NO_WTYPE;
    }
    return PB_OK;
}

inline static int
_unpack_object(struct _object* o, struct pb_slice* seri, struct pb_slice* c) {
    int r; 
    int number;
    int wtype;
    int oldpos;
    int wrap_number = -1;
_next:
    oldpos = seri->cur;
    if (_unpack_tag(seri, &number, &wtype))
        return PB_OK;

    if (number < 0 ||
        number > FIELD_NUMBER_MAX)
        return PB_ERR_NUMBER;

    if (number == wrap_number) {
        seri->cur = oldpos;
        return PB_OK;
    }

    struct _field* f = array_get(o->fields, number);
    if (f == NULL) {
        CHECK(_skip_field(wtype, seri));
        goto _next;
    }
    if (f->number != number) {
        return PB_ERR_VERIFY_NUMBER;
    } 
    if (f->wtype != wtype) {
        return PB_ERR_VERIFY_WTYPE;
    }
    if (wrap_number == -1)
        wrap_number = number;

    CHECK(_unpack_field(f, seri, c));
    goto _next;
}

/***********************************************************************************/
struct _type {
    const char* name;
    int ctype;
    int wtype;
    int bytes;
};

struct _label {
    char name;
    int label;
};
 
static struct _label _LABELS[] = {
    { 'Y', LABEL_REQUIRED },
    { 'O', LABEL_OPTIONAL },
    { 'R', LABEL_REPEATED },
    { 0, 0 },
};

static struct _type _TYPES[] = {
    { "bool",       CT_BOOL,    WT_VARINT, 1},
    { "uint8",      CT_UINT8,   WT_VARINT, 1},
    { "uint16",     CT_UINT16,  WT_VARINT, 2},
    { "uint32",     CT_UINT32,  WT_VARINT, 4},
    { "uint64",     CT_UINT64,  WT_VARINT, 8},
    { "int8",       CT_INT8,    WT_VARINT, 1},
    { "int16",      CT_INT16,   WT_VARINT, 2},
    { "int32",      CT_INT32,   WT_VARINT, 4},
    { "int64",      CT_INT64,   WT_VARINT, 8},
    { "fixed32",    CT_FIXED32, WT_BIT32,  4},
    { "fixed64",    CT_FIXED64, WT_BIT64,  8},
    { "sfixed32",   CT_SFIXED32,WT_BIT32,  4},
    { "sfixed64",   CT_SFIXED64,WT_BIT64,  8},
    { "float",      CT_FLOAT,   WT_BIT32,  4},
    { "double",     CT_DOUBLE,  WT_BIT64,  8},
    { "string",     CT_STRING,  WT_LEND,   1},
    { "bytes",      CT_BYTES,   WT_LEND,   1}, 
    { NULL, 0, 0 }
};

static int
_set_label(struct _field* f, char label) {
    struct _label* l;
    for (l=_LABELS; l->name; ++l) {
        if (l->name == label) {
            f->label = l->label;
            break;
        }
    }
    if (l->name)
        return PB_OK;
    else 
        return PB_ERR_LABEL;
}

static int
_set_type(struct _field* f, const char* type) {
    struct _type* t;
    for (t=_TYPES; t->name; ++t) {
        if (strcmp(t->name, type) == 0) {
            f->wtype = t->wtype;
            f->ctype = t->ctype; 
            f->bytes = t->bytes;
            f->tname = strdup(t->name);
            break;
        }
    }
    if (!t->name) {
        f->wtype = WT_LEND;
        f->ctype = CT_OBJECT;
        f->bytes = 0;
        f->tname = strdup(type);
    }
    if (f->label == LABEL_REPEATED) {
        f->wtype = WT_LEND;
    }
    return PB_OK;
}

static int
_set_repeat(struct _field* f, uint16_t repeat_max, int repeat_offset) {
    f->repeat_max = repeat_max;
    f->repeat_offset = repeat_offset;

    switch (f->label) {
    case LABEL_REPEATED:
        if (f->repeat_offset < 0 ||
            f->repeat_max <= 0)
            return PB_ERR_REPEAT;
        break;
    default:
        if (f->repeat_offset != 0)
            return PB_ERR_REPEAT;
        if (f->repeat_max == 0)
            f->repeat_max = 1;
        break;
    }
    if (f->repeat_max > 1) {
        f->wtype = WT_LEND;
    }
    return PB_OK;
}

static void
_delete_field(struct _field* f) {
    if (f) {
        free(f->tname);
        free(f->name);
        free(f);
    }
}

#define CHECK_ERR(f) *error = f; if (*error) goto err;

struct _field*
_new_field(struct pb_field_decl* fd, int* error) {
    int number = fd->number -1;
    if (number < 0 || 
        number > FIELD_NUMBER_MAX)
        return NULL;
    struct _field* f = malloc(sizeof(*f)); 
    memset(f, 0, sizeof(*f));
    f->number = number;
    f->offset = fd->offset;
    const char* type = fd->type;
    if (!*type) goto err;
    CHECK_ERR(_set_label(f, *type++));
    if (!*type) goto err;
    CHECK_ERR(_set_type(f, type));
    CHECK_ERR(_set_repeat(f, fd->repeat_max, fd->repeat_offset)); 
    if (f->bytes == 0)
        f->bytes = fd->bytes;
    f->name = strdup(fd->name);
    return f;
err:
    free(f);
    return NULL;
}

static void
_delete_object(struct _object* o) {
    if (o == NULL) return;
    size_t i;
    for (i=0; i<array_size(o->fields); ++i) {
        struct _field* f = array_get(o->fields, i);
        _delete_field(f);
    }
    free(o);
}


struct _object*
_new_object(const char* name, struct pb_field_decl* fds, size_t n, int* error) {
    struct _object* o = malloc(sizeof(*o));
    o->name = strdup(name);
    o->fields = array_new(n);

    struct _field* f;
    struct pb_field_decl* fd;
    size_t i;
    for (i=0; i<n; ++i) {
        fd = &fds[i];
        f = _new_field(fd, error);
        if (f == NULL)
            goto err;
        array_set(o->fields, f->number, f);
    }
    return o;
err:
    _delete_object(o);
    return NULL;
}

/**********************************************************************************/
struct pb_context {
    struct map* objects;
    char last_error[1024];
};

struct pb_context*
pb_context_new(size_t object_max) {
    struct pb_context* pbc = malloc(sizeof(*pbc));
    pbc->objects = map_new(object_max);
    pbc->last_error[0] = '\0';
    return pbc;
}

static void
_delete_cb(const char* key, void* value, void* ud) {
    struct _object* o = value;
    _delete_object(o);
}

void
pb_context_delete(struct pb_context* pbc) {
    if (pbc == NULL) return;
    if (pbc->objects) {
        map_foreach(pbc->objects, _delete_cb, NULL);
        map_free(pbc->objects);
    }
    free(pbc);
}

static void
_set_error(struct pb_context* pbc, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(pbc->last_error, sizeof(pbc->last_error), fmt, ap);
    pbc->last_error[sizeof(pbc->last_error)-1] = '\0';
    va_end(ap);
}

int
pb_context_object(struct pb_context* pbc, const char* name, 
        struct pb_field_decl* fds, size_t n) {
    int error;
    struct _object* o = _new_object(name, fds, n, &error);
    if (o) {
        map_insert(pbc->objects, o->name, o);
        return 0;
    } else {
        _set_error(pbc, _str_error(error));
        return 1;
    }
}

static void
_fresh_cb(const char* key, void* value, void* ud) {
    struct pb_context* pbc = ud;
    struct _object* o = value;
    size_t i;
    for (i=0; i<array_size(o->fields); ++i) {
        struct _field* f = array_get(o->fields, i);
        if (f &&
            f->ctype == CT_OBJECT) {
            if (f->child == NULL) {
                f->child = map_find(pbc->objects, f->tname);
            }
        }
    }
}

void
pb_context_fresh(struct pb_context* pbc) {
    map_foreach(pbc->objects, _fresh_cb, pbc);
}

int
_verify_object(struct pb_context* pbc, struct _object* o, int depth) {
    if (depth > 10) {
        _set_error(pbc, "object too depth");
        return 1;
    }
    size_t i;
    for (i=0; i<array_size(o->fields); ++i) {
        struct _field* f = array_get(o->fields, i);
        if (f &&
            f->ctype == CT_OBJECT) {
            if (f->child == NULL) {
                _set_error(pbc, "unknown object:%s", f->tname);
                return 1;
            }
            if (_verify_object(pbc, f->child, depth+1)) {
                return 1;
            }
        }
    }
    return 0;
}


static int
_verify_cb(const char* key, void* value, void* ud) {
    struct pb_context* pbc = ud;
    struct _object* o = value;
    return _verify_object(pbc, o, 1);
}

int
pb_context_verify(struct pb_context* pbc) {
    return map_foreach_check(pbc->objects, _verify_cb, pbc);
}

int
pb_context_seri(struct pb_context* pbc, const char* name, 
        struct pb_slice* c, struct pb_slice* seri) {
    struct _object* o = map_find(pbc->objects, name);
    if (o == NULL) {
        _set_error(pbc, "not found object:%s", name);
        return 1;
    }
    int error = _pack_object(o, c, seri);
    if (error) {
        _set_error(pbc, _str_error(error));
        return 1;
    }
    return 0;
}

int 
pb_context_unseri(struct pb_context* pbc, const char* name,
        struct pb_slice* seri, struct pb_slice* c) {
    struct _object* o = map_find(pbc->objects, name);
    if (o == NULL) {
        _set_error(pbc, "not found object:%s", name);
        return 1;
    }
    int error = _unpack_object(o, seri, c);
    if (error) {
        _set_error(pbc, _str_error(error));
        return 1;
    }
    return 0;

}

static void
_dump_cb(const char* key, void* value, void* ud) {
    struct _object* o = value;
    printf("message %s {\n", o->name);
    size_t i;
    for (i=0; i<array_size(o->fields); ++i) {
        struct _field* f = array_get(o->fields, i);
        if (f == NULL) continue;
        char label = '?';
        struct _label* l;
        for (l=_LABELS; l->name; ++l) {
            if (l->label == f->label) {
                label = l->name;
                break;
            }
        }
        int number = f->number+1;
        if (f->label == LABEL_REPEATED) {
            if (f->repeat_offset > 0) {
                printf("%c %s %s[max=%d] = %d;\n", 
                        label, 
                        f->tname, 
                        f->name, 
                        f->repeat_max, 
                        number);
            } else {
                printf("%c %s %s[%d] = %d;\n", 
                        label, 
                        f->tname, 
                        f->name, 
                        f->repeat_max, 
                        number);

            }
        } else {
            printf("%c %s %s = %d;\n", label, 
                    f->tname, f->name, number);
        }
    }
    printf("}\n");
}

void
pb_context_dump(struct pb_context* pbc) {
    map_foreach(pbc->objects, _dump_cb, NULL);
}

const char*
pb_context_lasterror(struct pb_context* pbc) {
    return pbc->last_error;
}
