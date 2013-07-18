#include "pb.h"
#include "array.h"
#include "map.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "pb_init.h"

void test_pb() {
    struct pb_context* pbc = PB_CONTEXT_INIT();
    pb_context_dump(pbc);

    char sbuffer[1024];
    struct pb_slice s;
    s.pointer = sbuffer;
    s.size = sizeof(sbuffer);
    s.cur = 0;

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
    c.cur = 0;
    if (pb_context_seri(pbc, "Hero", &c, &s)) {
        printf("seri error: %s", pb_context_lasterror(pbc));
        return;
    }
    printf("seri size: %lu -> %zu\n", sizeof(h), s.cur);

    struct Hero h2;
    memset(&h2, 0, sizeof(h2));

    struct pb_slice c2;
    c2.pointer = (char*)&h2;
    c2.size = sizeof(h2);
    c2.cur = 0;
    s.size = s.cur;
    s.cur = 0;
    if (pb_context_unseri(pbc, "Hero", &s, &c2)) {
        printf("unseri error: %s", pb_context_lasterror(pbc));
        return;
    }
    assert(c.size == c2.size);
    assert(memcmp(&h, &h2, sizeof(h)) == 0);

    printf("pb test ok\n");
    pb_context_delete(pbc);

}

int 
main(int argc, char* argv[]) {
    test_pb();
    return 0;
}
