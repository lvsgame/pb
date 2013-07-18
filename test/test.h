/*this file is generate by protoc.lua do not change it by hand*/

#ifndef __pb_test_h__
#define __pb_test_h__

#include <stdint.h>


// hero attribute
struct Attribute {
    uint32_t hp;
    uint32_t mp;
    uint32_t power;
    uint16_t nbox;
    float box[10];
};

// hero equip
struct Equip {
    uint32_t head;
    uint32_t hand;
};

// hero
struct Hero {
    char name[32];
    uint32_t id;
    uint16_t nattris;
    struct Attribute attris[10];
    struct Equip equip;
};
/*
message Repeat {
    required uint32 a=1;
}
*/
enum Camp {
    Camp_1 = 1,
    Camp_2 = 2,
    Camp_Max,
};

#endif