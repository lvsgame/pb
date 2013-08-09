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
  test.pb.h
 }
 tool=tool {
  pathconfig.lua
  fs.lua
  lfs.so
  lpeg.so
  plp.lua
  protoc.lua
 }
}
