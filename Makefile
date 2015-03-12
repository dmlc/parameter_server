CC = g++

# OPT = -O0 -ggdb
OPT = -O3 -ggdb

THIRD_PATH=$(shell pwd)/third_party
THIRD_INC=-I$(shell pwd)/third_party/include
STATIC_THIRD_LIB=0
ifeq ($(STATIC_THIRD_LIB), 1)
THIRD_LIB=$(addprefix $(THIRD_PATH)/lib/, libgflags.a libzmq.a libprotobuf.a libglog.a libz.a  libsnappy.a)
else
THIRD_LIB=-L$(THIRD_PATH)/lib -lgflags -lzmq -lprotobuf -lglog -lz -lsnappy
endif
# THIRD_LIB+=-ltcmalloc_and_profiler

# TODO: detect the Python version
THIRD_INC+=-I/usr/include/python2.7
THIRD_LIB+=-lpython2.7
# borrow boost-python and boost-numpy from Minerva
THIRD_INC+=-I../minerva/deps/include
THIRD_LIB+=-L../minerva/deps/lib
THIRD_LIB+=-lboost_python -lboost_numpy

WARN = -Wall -Wno-unused-function -finline-functions -Wno-sign-compare #-Wconversion
INCPATH = -I./src $(THIRD_INC)
CFLAGS = -std=c++0x $(WARN) $(OPT) $(INCPATH)
LDFLAGS += $(THIRD_LIB) -lpthread -lrt

PS_LIB = build/libps.a
PS_MAIN = build/libpsmain.a

all: ps app
clean:
	rm -rf build

ps: $(PS_LIB) $(PS_MAIN)
app: build/ps

build/hello: build/app/hello_world/main.o $(PS_LIB) $(PS_MAIN)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

build/ps_python: build/app/python/main.o build/app/python/python_env.o build/app/python/python_bindings.o $(PS_LIB) $(PS_MAIN)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

sys_srcs	= $(wildcard src/util/*.cc) $(wildcard src/data/*.cc) \
			  $(wildcard src/system/*.cc) $(wildcard src/filter/*.cc)
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
-include src/test/build.mk
