CC = g++

OPT = -O3 -ggdb
# OPT = -O0 -ggdb

BIN = ../bin
LIB = ../lib

ifndef THIRD
THIRD=$(shell cd ..;pwd)/third_party
endif
THIRD_BIN=$(THIRD)/bin
THIRD_INC=$(THIRD)/include
THIRD_LIB=$(THIRD)/lib

#-Wconversion -fPIC
WARN = -Wall -Wno-unused-function -finline-functions -Wno-sign-compare
INCPATH = -I. -I${THIRD_INC}

# STATIC=1
ifeq (${STATIC}, 1)
# TODO a lot of warning on mac with llvm
    CFLAGS = -static -static-libgcc -static-libstdc++ -std=c++0x $(WARN) $(OPT) $(INCPATH) -fPIC # -fopenmp
else
    CFLAGS = -std=c++0x $(WARN) $(OPT) $(INCPATH) #-fopenmp #-fPIC #
endif

LDFLAGS = -L$(THIRD_LIB) -L$(LIB) -lps -lgflags -lzmq -lprotobuf -lglog -lz -lsnappy -lm -lpthread -lrt -lstdc++
GTEST = -lgtest_main -lgtest

OBJS = $(SRCS:.cc=.o)
DEPS = $(SRCS:.cc=.d)
PROTOS= $(wildcard proto/*.proto)
PROTOS_CC = $(PROTOS:.proto=.pb.cc)
SRCS = $(PROTOS_CC) \
	util/crc32c.cc \
	util/MurmurHash3.cc \
	util/file.cc \
	util/filelinereader.cc \
	util/threadpool.cc \
	system/van.cc \
	system/message.cc \
	system/postoffice.cc \
	system/yellow_pages.cc \
	system/remote_node.cc \
	system/executor.cc \
	system/app.cc \
	data/text_parser.cc \
	linear_method/linear_method.cc \
	linear_method/batch_solver.cc \
	linear_method/darling.cc


TESTS = \
	matrix_io_test \
	localizer_test \
	countmin_test \
	# sparse_matrix_perf \
	# auc_test \
	io_test \
	bitmap_test \
	shared_array_test \
	eigen3_test \
	sparse_matrix_test \
	auc_layer_test \
	# double_test \
	# aggregated_gradient_test \
	# grad_desc_test \
	# strtonum_test
	#partition_test
	# van_test \
	# customer_test \
	# vectors_test \
        sparse_matrix_multi_thread_perf \
	fault_tolerance_press \

all: dir $(addprefix $(BIN)/, ps) # text2proto
data: $(addprefix $(BIN)/, showins proto2bin)
test: $(addprefix $(BIN)/, $(TESTS))

dir:
	mkdir -p $(LIB)
	mkdir -p $(BIN)
clean:
	rm -rf */*.o */*.d */*/*.o */*/*.d proto/*.cc $(BIN) $(LIB)

%.o: %.cc %.h
	$(CC) $(CFLAGS) -c $< -o $@

%.d: %.cc %.h
	$(CC) $(CFLAGS) -MM -MT $*.o $< > $@

$(BIN)/%: data/%.cc $(LIB)/libps.a
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

$(BIN)/%: test/%.cc $(LIB)/libps.a
	$(CC) $(CFLAGS) $< $(GTEST) $(LDFLAGS) -o $@

./proto/%.pb.cc ./proto/%.pb.h : ./proto/%.proto
	${THIRD_BIN}/protoc --cpp_out=. $<

-include $(DEPS)

# static link, works fine for both mac and linux.
$(LIB)/libps.a: $(OBJS) #gtest
	ar crv $@ $(OBJS)

$(BIN)/ps: ps.cc $(LIB)/libps.a
	$(CC) $(CFLAGS) ps.cc $(LDFLAGS) -o $@

# =====================

# -Wl,-rpath=../lib $(LIB)/libps.so
