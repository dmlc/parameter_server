CC = g++

OPT = -O0 -ggdb
# OPT = -O3 -ggdb

BIN = ../bin
LIB = ../lib

ifndef THIRD
THIRD=$(shell cd ..;pwd)/third_party
endif
THIRD_BIN=$(THIRD)/bin
THIRD_INC=$(THIRD)/include
THIRD_LIB=$(THIRD)/lib

#-Wconversion -fPIC
WARN = -Wall -Wno-unused-function -finline-functions -Wno-sign-compare #-fno-omit-frame-pointer
INCPATH = -I. -I${THIRD_INC}

# STATIC=1
ifeq (${STATIC}, 1)
# TODO a lot of warning on mac with llvm
    CFLAGS = -static -static-libgcc -static-libstdc++ -std=c++0x $(WARN) $(OPT) $(INCPATH) -fPIC # -fopenmp
else
    CFLAGS = -std=c++0x $(WARN) $(OPT) $(INCPATH) #-fopenmp #-fPIC #
endif

LDFLAGS = -L$(THIRD_LIB) -L$(LIB) -lps -lgflags -lzmq -lprotobuf -lglog -lz \
-lsnappy -lm -lpthread  -lstdc++ # -ltcmalloc_and_profiler # -lrt
GTEST = -lgtest_main -lgtest

OBJS = $(SRCS:.cc=.o)
PROTOS= $(wildcard */*.proto)
PROTOS_CC = $(PROTOS:.proto=.pb.cc)

-include */BUILD
SRCS = $(PROTOS_CC) \
	$(addprefix graph_partition/, $(graph_partition_srcs)) \
	$(addprefix system/, $(system_srcs)) \
	$(addprefix util/, $(util_srcs)) \
	$(addprefix data/, $(data_srcs)) \
	$(addprefix linear_method/, $(linear_method_srcs)) \
	filter/filter.cc


TESTS = \
	stream_reader_test \
	slot_reader_test \
	matrix_io_test \
	localizer_test \
	countmin_test \
	snappy_test \
	shared_array_test \
	grad_desc_test \
	ftrl_test \
	prepare_test_data \
	bloom_filter_test \
	test
	# sparse_matrix_perf \
	auc_test \
	io_test \
	bitmap_test \
	eigen3_test \
	sparse_matrix_test \
	auc_layer_test \
	# double_test \
	# aggregated_gradient_test \
	# strtonum_test
	#partition_test
	# van_test \
	# customer_test \
	# vectors_test \
        sparse_matrix_multi_thread_perf \
	fault_tolerance_press \

ALL_SRCS = $(SRCS) $(addsuffix .cc, $(addprefix test/, $(TESTS)))
DEPS = $(ALL_SRCS:.cc=.d)

all: dir $(addprefix $(BIN)/, ps)

clean:
	rm -rf */*.o */*.d */*/*.o */*/*.d proto/*.cc $(BIN) $(LIB)

test: $(addprefix $(BIN)/, $(TESTS))

%.o: %.cc
	$(CC) $(CFLAGS) -c $< -o $@

%.d: %.cc
	$(CC) $(CFLAGS) -MM -MT $*.o $< > $@

$(BIN)/%: data/%.cc $(LIB)/libps.a
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

$(BIN)/%: test/%.cc $(LIB)/libps.a
	$(CC) $(CFLAGS) $< $(GTEST) $(LDFLAGS) -o $@

%.pb.cc %.pb.h : %.proto
	${THIRD_BIN}/protoc --cpp_out=. $<

# static link, works fine for both mac and linux.
$(LIB)/libps.a: $(OBJS) #gtest
	ar crv $@ $(OBJS)

$(BIN)/ps: main.cc $(LIB)/libps.a
	$(CC) $(CFLAGS) main.cc $(LDFLAGS) -o $@

# make directories
dir: $(LIB) $(BIN)
$(LIB):
	mkdir -p $(LIB)
$(BIN):
	mkdir -p $(BIN)

-include $(DEPS)

###  tmp junks

# t: ../bin/slot_reader_test
# t: ../bin/prepare_test_data
t: ../bin/bloom_filter_test/
../bin/bloom_filter_test: test/bloom_filter_test.o
	$(CC) $(CFLAGS) $< -lgtest $(LDFLAGS) -o $@

e: ../bin/test

# -Wl,-rpath=../lib $(LIB)/libps.so
# data: $(addprefix $(BIN)/, showins proto2bin)
