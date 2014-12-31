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
LIBS = $(addprefix $(LIB)/, liblm.a libgp.a libps.a libprotos.a)
LDFLAGS += -L$(LIB) $(LIBS) $(THIRD_LIBS) -lpthread -lrt

all: dir $(BIN)/ps
clean:
	rm -rf */*.o */*.d */*/*.o */*/*.d proto/*.cc $(BIN) $(LIB)

-include */BUILD

#$(PROTOS_CC) \
### system
SYS_SRCS = \
	$(addprefix system/, $(system_srcs)) \
	$(addprefix util/, $(util_srcs)) \
	$(addprefix data/, $(data_srcs)) \
	$(addprefix filter/, $(filter_srcs))
SYS_OBJS = $(SYS_SRCS:.cc=.o)
$(LIB)/libps.a: $(SYS_OBJS)
	ar crv $@ $(SYS_OBJS)

### linear methods
LM_SRCS = $(addprefix linear_method/, $(linear_method_srcs))
LM_OBJS = $(LM_SRCS:.cc=.o)
$(LIB)/liblm.a: $(LM_OBJS)
	ar crv $@ $(LM_OBJS)

### graph partitioning
GP_SRCS = $(addprefix graph_partition/, $(graph_partition_srcs))
GP_OBJS = $(GP_SRCS:.cc=.o)
$(LIB)/libgp.a: $(GP_OBJS)
	ar crv $@ $(GP_OBJS)


### protobuf
PROTOS= $(wildcard */*.proto)
PROTOS_CC = $(PROTOS:.proto=.pb.cc)
PROTOS_OBJS = $(PROTOS:.proto=.pb.o)
%.pb.cc %.pb.h : %.proto
	${THIRD_BIN}/protoc --cpp_out=. $<
$(LIB)/libprotos.a: $(PROTOS_OBJS)
	ar crv $@ $(PROTOS_OBJS)

### dependencies
ALL_SRCS = $(wildcard */*.cc)
DEPS = $(ALL_SRCS:.cc=.d)
-include $(DEPS)

### rules
test: $(addprefix $(BIN)/, $(TESTS))
%.o: %.cc
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
