pb=~/pb CD=. filter="*" {
 Makefile
 README.md
 pb
 src=src {
  map.c
  pb.c
  array.h
  map.h
  pb.h
 }
 test=test {
  test.c
  test.proto
  pb_log.h
 }
 tool=tool {
  fs.lua
  pathconfig.lua
  plp.lua
  protoc.lua
 }
}
