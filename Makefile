.PHONY: all test pb clean

CFLAGS = -Werror -O2
SHARED = -fPIC -shared

all : \
	build/libpb.so \
	build/test

build :
	mkdir build

pb : test/pb_init.h

build/libpb.so: src/pb.c src/pb.h src/array.h src/map.c src/map.h | build
	gcc $(CFLAGS) $(SHARED) -o $@ $^

PROTOD = $(wildcard test/*.proto)
PROTOH = $(PROTOD:%.proto=%.pb.h)

test/pb_init.h : $(PROTOD)
	cd tool && lua protoc.lua ../test ../test ../$@

build/test: test/pb_init.h $(PROTOH) test/test.c 
	gcc $(CFLAGS) -o $@ $^ -lrt -lpb -L./build -I./src

clean:
	rm -rf build test/pb_init.h

cleanall: clean
	rm -f cscope.* tags
	find -name ".*.swp"|xargs rm -rf
