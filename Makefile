CC = g++

OPT = -O0 -ggdb
# OPT = -O3 -ggdb

THIRD_PATH=$(shell pwd)/third_party
STATIC_THIRD_LIB=0
ifeq ($(STATIC_THIRD_LIB), 1)
THIRD_LIB=$(addprefix $(THIRD_PATH)/lib, libgflags.a libzmq.a libprotobuf.a libglog.a libz.a  libsnappy.a)
else
THIRD_LIB=-L$(THIRD_PATH)/lib -lgflags -lzmq -lprotobuf -lglog -lz -lsnappy
endif

WARN = -Wall -Wno-unused-function -finline-functions -Wno-sign-compare #-Wconversion
INCPATH = -I./src -I$(THIRD_PATH)/include
CFLAGS = -std=c++0x $(WARN) $(OPT) $(INCPATH)
LDFLAGS += ./build/libps.a $(THIRD_LIB) -lpthread -lrt

all: ps build/hello
clean:
	rm -rf build

ps: build/libps.a

build/hello: build/app/hello_world/main.o build/libps.a
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@


app:
	echo xx

sys_srcs	= $(wildcard src/*/*.cc)
sys_protos	= $(wildcard src/*/proto/*.proto)
sys_objs	= $(patsubst src/%.proto, build/%.pb.o, $(sys_protos)) \
		      $(patsubst src/%.cc, build/%.o, $(sys_srcs))
build/libps.a: $(sys_objs)
	ar crv $@ $?


build/%.o: src/%.cc
	@mkdir -p $(@D)
	$(CC) $(INCPATH) -std=c++0x -MM -MT build/$*.o $< >build/$*.d
	$(CC) $(CFLAGS) -c $< -o $@

%.pb.cc %.pb.h : %.proto
	${THIRD_PATH}/bin/protoc --cpp_out=./src --proto_path=./src $<

-include build/*/*.d

# test :
# 	make -C src test -j8
# clean :
# 	make -C src clean
