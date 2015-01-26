CC = g++-4.8

# OPT = -O0 -ggdb
OPT = -O3 -ggdb

THIRD_PATH=$(shell pwd)/third_party
STATIC_THIRD_LIB=1
ifeq ($(STATIC_THIRD_LIB), 1)
THIRD_LIB=$(addprefix $(THIRD_PATH)/lib/, libgflags.a libzmq.a libprotobuf.a libglog.a libz.a  libsnappy.a)
else
THIRD_LIB=-L$(THIRD_PATH)/lib -lgflags -lzmq -lprotobuf -lglog -lz -lsnappy
endif
# THIRD_LIB+=-ltcmalloc_and_profiler

WARN = -Wall -Wno-unused-function -finline-functions -Wno-sign-compare #-Wconversion
INCPATH = -I./src -I$(THIRD_PATH)/include
CFLAGS = -std=c++0x $(WARN) $(OPT) $(INCPATH)
LDFLAGS += $(THIRD_LIB) -lpthread -lrt

PS_LIB = build/libps.a
PS_MAIN = build/libpsmain.a

all: ps app build/hello
clean:
	rm -rf build

ps: $(PS_LIB) $(PS_MAIN)
app: build/ps

build/hello: build/app/hello_world/main.o $(PS_LIB) $(PS_MAIN)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

sys_srcs	= $(wildcard src/*/*.cc)
sys_protos	= $(wildcard src/*/proto/*.proto)
sys_objs	= $(patsubst src/%.proto, build/%.pb.o, $(sys_protos)) \
		      $(patsubst src/%.cc, build/%.o, $(sys_srcs))
build/libps.a: $(sys_objs)
	ar crv $@ $?

build/libpsmain.a: build/ps_main.o
	ar crv $@ $?

app_objs = $(addprefix build/app/, main/proto/app.pb.o linear_method/linear.o linear_method/proto/linear.pb.o)

build/ps:  build/app/main/main.o $(app_objs) $(PS_LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

build/%.o: src/%.cc
	@mkdir -p $(@D)
	$(CC) $(INCPATH) -std=c++0x -MM -MT build/$*.o $< >build/$*.d
	$(CC) $(CFLAGS) -c $< -o $@

%.pb.cc %.pb.h : %.proto
	${THIRD_PATH}/bin/protoc --cpp_out=./src --proto_path=./src $<

-include build/*/*.d
-include build/*/*/*.d
