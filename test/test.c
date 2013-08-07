#include "pb.h"
#include "array.h"
#include "map.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include "pb_init.h"

uint64_t _time() {
    struct timespec ti;
    clock_gettime(CLOCK_MONOTONIC, &ti);
    return ti.tv_sec * 1000 + ti.tv_nsec / 1000000;
}

void test_pb() {
    struct pb_context* pbc = PB_CONTEXT_INIT();
    pb_context_dump(pbc);

    char sbuffer[1024];
    struct pb_slice s;
    s.pointer = sbuffer;
    s.size = sizeof(sbuffer);

    struct Hero h;
    memset(&h, 0, sizeof(h));
    strncpy(h.name, "test hero", sizeof(h.name));
    h.id = 0xffffff;
    h.nattris = 5;
    int i;
    for (i=0; i<h.nattris; ++i) {
        h.attris[i].hp = (i+1) * 1000;
        h.attris[i].mp = (i+1) * 2000;
        h.attris[i].power = (i+1) * 3000;
        h.attris[i].nbox = i;
        int j;
        for (j=0; j<h.attris[i].nbox; ++j) {
            h.attris[i].box[j] = (j+1)*1.234f;
        }
    }
    h.equip.head = 100001;
    h.equip.hand = 200003;

    struct pb_slice c;
    c.pointer = (char*)&h;
    c.size = sizeof(h);
    if (pb_context_seri(pbc, PBO_HERO, &c, &s)) {
        printf("seri error: %s", pb_context_lasterror(pbc));
        return;
    }
    printf("seri size: %lu -> %zu\n", sizeof(h), s.cur-s.pointer);

    struct Hero h2;
    memset(&h2, 0, sizeof(h2));

    struct pb_slice c2;
    c2.pointer = (char*)&h2;
    c2.size = sizeof(h2);
    s.size = s.cur-s.pointer;
    if (pb_context_unseri(pbc, PBO_HERO, &s, &c2)) {
        printf("unseri error: %s", pb_context_lasterror(pbc));
        return;
    }
    assert(c.size == c2.size);
    assert(memcmp(&h, &h2, sizeof(h)) == 0);

    printf("pb test ok\n");
    pb_context_delete(pbc);

}

void benchmark(int times) {
    struct pb_context* pbc = PB_CONTEXT_INIT();
    pb_context_dump(pbc);
 
    struct Hero h;
    memset(&h, 0, sizeof(h));
    strncpy(h.name, "test hero", sizeof(h.name));
    h.id = 0xffffff;
    h.nattris = 5;
    int i;
    for (i=0; i<h.nattris; ++i) {
        h.attris[i].hp = (i+1) * 1000;
        h.attris[i].mp = (i+1) * 2000;
        h.attris[i].power = (i+1) * 3000;
        h.attris[i].nbox = i;
        int j;
        for (j=0; j<h.attris[i].nbox; ++j) {
            h.attris[i].box[j] = (j+1)*1.234f;
        }
    }
    h.equip.head = 100001;
    h.equip.hand = 200003;

    char sbuffer[1024];
    struct pb_slice s;
    struct pb_slice c;
    struct pb_slice c2;

    uint64_t t1 = _time();
    for (i=0; i<times; ++i) {  
        s.pointer = sbuffer;
        s.size = sizeof(sbuffer);
        c.pointer = (char*)&h;
        c.size = sizeof(h);

        if (pb_context_seri(pbc, PBO_HERO, &c, &s)) {
            printf("seri error: %s", pb_context_lasterror(pbc));
            return;
        }
    }
    uint64_t t2 = _time();
    printf("seri times=%d, time=%lu, size: %lu -> %zu\n", 
            times, t2-t1, sizeof(h), s.cur-s.pointer);

    struct Hero h2;
    t1 = _time();
    for (i=0; i<times; ++i) { 
        memset(&h2, 0, sizeof(h2));
 
        c2.pointer = (char*)&h2;
        c2.size = sizeof(h2);
        s.size = s.cur-s.pointer;
        if (pb_context_unseri(pbc, PBO_HERO, &s, &c2)) {
            printf("unseri error: %s", pb_context_lasterror(pbc));
            return;
        }
    }
    t2 = _time();
    printf("unseri times=%d, time=%lu, size: %lu -> %zu\n", 
            times, t2-t1, s.cur-s.pointer, sizeof(h2));

    assert(c.size == c2.size);
    assert(memcmp(&h, &h2, sizeof(h)) == 0);

    t1 = _time();
    int j;
    for (i=0; i<times; ++i) {
        strncpy(h2.name, h.name, sizeof(h.name));
        h2.id = h.id;
        h2.nattris = h.nattris;
        for (j=0; j<10; ++j) {
            h2.attris[j] = h.attris[j];
        }
        h2.equip = h.equip;
    }
    t2 = _time();
    printf("memcpy times=%d, time=%lu\n", times, t2-t1);

    struct Test test;
    memset(&test, 0, sizeof(test));
    t1 = _time();
    for (i=0; i<times; ++i) {  
        
        test.id=1000;
        test.id2=2000;
        test.id3=3000;
        test.id4=4000;
        test.id5=5000;
        test.id6=6000;
        test.id7=7000;
        test.id8=8000;
        test.id9=9000;
        test.id10=10000;

        s.pointer = sbuffer;
        s.size = sizeof(sbuffer);
        c.pointer = (char*)&test;
        c.size = sizeof(test);

        if (pb_context_seri(pbc, PBO_TEST, &c, &s)) {
            printf("seri error: %s", pb_context_lasterror(pbc));
            return;
        }
    }
    t2 = _time();
    printf("test seri times=%d, time=%lu, size: %lu -> %zu\n", 
            times, t2-t1, sizeof(test), s.cur-s.pointer);

    struct Test test2;
    memset(&test2, 0, sizeof(test2));
    t1 = _time();
    for (i=0; i<times; ++i) {  
        s.size = s.cur-s.pointer;
        c.pointer = (char*)&test2;
        c.size = sizeof(test2);

        if (pb_context_unseri(pbc, PBO_TEST, &s, &c)) {
            printf("seri error: %s", pb_context_lasterror(pbc));
            return;
        }
    }
    t2 = _time();
    printf("test unseri times=%d, time=%lu, size: %zu -> %lu\n", 
            times, t2-t1, s.cur-s.pointer, sizeof(test2));
    assert(memcmp(&test, &test2, sizeof(test)) == 0);
    pb_context_delete(pbc);

}

int 
main(int argc, char* argv[]) {
    test_pb();
    if (argc > 1) {
        int times = strtol(argv[1], NULL, 10);
        benchmark(times);
    }
    return 0;
}
