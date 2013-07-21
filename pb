pb=~/pb CD=. filter="*" {
 Makefile
 pb
 src=src {
  array.c
  map.c
  pb.c
  array.h
  map.h
  pb.h
 }
 test=test {
  test.c
  test.proto
  pb_init.h
  pb_log.h
  test.h
 }
 tool=tool {
  lfs.so
  lpeg.so
  plp.lua
  protoc.lua
 }
}
