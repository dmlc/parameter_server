CC = g++

# OPT = -O0 -ggdb
OPT = -O3 -ggdb

### third_party path
THIRD_PARTY=$(shell cd ..;pwd)/third_party
THIRD_BIN=$(THIRD_PARTY)/bin
THIRD_INC=$(THIRD_PARTY)/include
THIRD_LIB=$(THIRD_PARTY)/lib

WARN = -Wall -Wno-unused-function -finline-functions -Wno-sign-compare #-Wconversion
INCPATH = -I. -I${THIRD_INC}
CFLAGS = -std=c++0x $(WARN) $(OPT) $(INCPATH)

### staticly link third party libaries
STATIC_THIRD_LIB=0
ifeq ($(STATIC_THIRD_LIB), 1)
THIRD_LIBS=$(addprefix $(THIRD_LIB)/, libgflags.a libzmq.a libprotobuf.a libglog.a libz.a  libsnappy.a)
else
THIRD_LIBS=-L$(THIRD_LIB) -lgflags -lzmq -lprotobuf -lglog -lz -lsnappy
endif

### staticly link gcc
STATIC_GCC=0
ifeq (${STATIC_GCC}, 1)
	LDFLAGS += -static-libgcc -static-libstdc++
endif

### all
BIN = ../bin
LIB = ../lib
# cxxnet.a
LIBS = $(addprefix $(LIB)/, lm.a ps.a proto.a)
LDFLAGS += -L$(LIB) $(LIBS) $(THIRD_LIBS) -lpthread -lrt
# LDFLAGS += -ltcmalloc_and_profiler

all: dir $(BIN)/ps
clean:
	rm -rf */*.o */*.d */*/*.o */*/*.d */*.pb.cc */*/*.pb.cc $(BIN) $(LIB)

-include */BUILD

### system
sys_srcs = \
	$(addprefix system/, $(system_srcs)) \
	$(addprefix util/, $(util_srcs)) \
	$(addprefix data/, $(data_srcs)) \
	$(addprefix filter/, $(filter_srcs))
$(LIB)/ps.a: $(sys_srcs:.cc=.o)
	ar crv $@ $?

### linear methods
$(LIB)/lm.a: $(linear_method_srcs:.cc=.o)
	ar crv $@ $?

### cxxnet
CFLAGS += -I../plugin/mshadow -I/opt/intel/mkl/include \
	-Wno-unknown-pragmas -funroll-loops -DMSHADOW_USE_CUDA=0
LDFLAGS += -L/opt/intel/mkl/lib/intel64
$(LIB)/cxxnet.a: $(cxxnet_srcs:.cc=.o)
	ar crv $@ $?

$(LIB)/gp.a: $(graph_partition_srcs:.cc=.o)
	ar crv $@ $?

$(LIB)/fm.a: $(graph_partition_srcs:.cc=.o)
	ar crv $@ $?

### protobuf
PROTOS= $(wildcard */*/*.proto) $(wildcard */*.proto)
%.pb.cc %.pb.h : %.proto
	${THIRD_BIN}/protoc --cpp_out=. $<
$(LIB)/proto.a: $(PROTOS:.proto=.pb.o)
	ar crv $@ $?


### dependencies
ALL_SRCS = $(wildcard */*.cc) $(wildcard */*/*.cc)
DEPS = $(ALL_SRCS:.cc=.d)
-include $(DEPS)

### rules
test: $(addprefix $(BIN)/, $(TESTS))
%.o: %.cc %.h
	$(CC) $(CFLAGS) -c $< -o $@
%.d: %.cc
	$(CC) $(CFLAGS) -MM -MT $*.o $< > $@
$(BIN)/%: test/%.cc $(LIBS)
	$(CC) $(CFLAGS) $< -lgtest_main -lgtest $(LDFLAGS) -o $@
$(BIN)/ps: main.cc $(LIBS)
	$(CC) $(CFLAGS) main.cc $(LDFLAGS) -o $@


### make directories
dir: $(LIB) $(BIN)
$(LIB):
	mkdir -p $(LIB)
$(BIN):
	mkdir -p $(BIN)



# PROTOS_CC = $(PROTOS:.proto=.pb.cc)
# PROTOS_OBJS = $(PROTOS:.proto=.pb.o)
