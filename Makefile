ifneq ("$(wildcard ./config.mk)","")
include ./config.mk
else
include make/config.mk
endif

ifeq ($(STATIC_THIRD_LIB), 1)
THIRD_LIB=$(addprefix $(THIRD_PATH)/lib/, libgflags.a libzmq.a libprotobuf.a libglog.a libz.a  libsnappy.a)
	ifeq ($(USE_S3),1)
	THIRD_LIB+=$(addprefix $(THIRD_PATH)/lib/, libxml2.a)
	endif
else
THIRD_LIB=-L$(THIRD_PATH)/lib -lgflags -lzmq -lprotobuf -lglog -lz -lsnappy
	ifeq ($(USE_S3),1)
	THIRD_LIB+=-lxml2
	endif
endif

WARN = -Wall -Wno-unused-function -finline-functions -Wno-sign-compare #-Wconversion
INCPATH = -I./src -I$(THIRD_PATH)/include
<<<<<<< HEAD
CFLAGS = -std=c++0x $(WARN) $(OPT) $(INCPATH)
LDFLAGS += $(THIRD_LIB) -lpthread
OS := $(shell uname -s)
ifeq ($(OS),Linux)
	LFLAGS += -lrt
endif
=======
CFLAGS = -std=c++0x $(WARN) $(OPT) $(INCPATH) $(EXTRA_CFLAGS)
ifeq ($(USE_S3), 1)
CFLAGS += -DUSE_S3=1
endif
LDFLAGS = $(EXTRA_LDFLAGS) $(THIRD_LIB) -lpthread # -lrt
>>>>>>> upstream/master

PS_LIB = build/libps.a
PS_MAIN = build/libpsmain.a
TEST_MAIN = build/test_main.o

clean:
	rm -rf build
	find src -name "*.pb.[ch]*" -delete

ps: $(PS_LIB) $(PS_MAIN) $(TEST_MAIN)

# PS system
sys_dir		= $(addprefix src/, util data system filter learner parameter)
sys_srcs	= $(wildcard $(patsubst %, %/*.cc, $(sys_dir)))
sys_protos	= $(wildcard $(patsubst %, %/proto/*.proto, $(sys_dir)))
sys_objs	= $(patsubst src/%.proto, build/%.pb.o, $(sys_protos)) \
			  $(patsubst src/%.cc, build/%.o, $(sys_srcs))

build/libps.a: $(patsubst %.proto, %.pb.h, $(sys_protos)) $(sys_objs)
	ar crv $@ $(filter %.o, $?)

build/libpsmain.a: build/ps_main.o
	ar crv $@ $?

# applications
build/linear: $(addprefix build/app/linear_method/, proto/linear.pb.o main.o) $(PS_LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# general rules
build/%.o: src/%.cc
	@mkdir -p $(@D)
	$(CC) $(INCPATH) -std=c++0x -MM -MT build/$*.o $< >build/$*.d
	$(CC) $(CFLAGS) -c $< -o $@

%.pb.cc %.pb.h : %.proto
	${THIRD_PATH}/bin/protoc --cpp_out=./src --proto_path=./src $<

-include build/*/*.d
-include build/*/*/*.d
-include src/test/build.mk
