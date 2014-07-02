CC = g++
# CC = clang

OPT = -O3 -ggdb
# OPT = -O0 -ggdb

BIN = ../bin
LIB = ../lib

THIRD=$(shell cd ..;pwd)/third_party
# THIRD=/usr/local/

THIRD_BIN=$(THIRD)/bin
THIRD_INC=$(THIRD)/include
THIRD_LIB=$(THIRD)/lib

# MPICC = mpic++
MPICC = $(THIRD_BIN)/mpic++

#-Wconversion -fPIC
WARN = -Wall -Wno-unused-function -finline-functions -Wno-sign-compare
INCPATH = -I. -I${THIRD_INC}

ifeq (${args}, static)
# TODO a lot of warning on mac with llvm
    CFLAGS = -static -static-libgcc -static-libstdc++ -std=c++0x $(WARN) $(OPT) $(INCPATH) -fPIC # -fopenmp
else
    CFLAGS = -std=c++0x $(WARN) $(OPT) $(INCPATH) #-fPIC # -fopenmp
endif

LDFLAGS = -L$(THIRD_LIB) -L$(LIB) -lps \
-lgflags -lzmq -lprotobuf -lglog -lz -lsnappy -lmpi -lm -lpthread -lstdc++

GTEST =  -lgtest_main -lgtest

PROTOS= $(wildcard proto/*.proto)
PROTOS_CC = $(PROTOS:.proto=.pb.cc)
SRCS = $(PROTOS_CC) \
	util/crc32c.cc \
	util/MurmurHash3.cc \
	util/file.cc \
	util/filelinereader.cc \
	util/recordio.cc \
	util/threadpool.cc \
	system/van.cc \
	system/postoffice.cc \
	system/yellow_pages.cc \
	system/remote_node.cc \
	system/executor.cc \
	app/app.cc \
	app/risk_minimization.cc \
	app/linear_method.cc \
	app/linear_block_iterator.cc \
	app/block_coordinate_l1lr.cc

OBJS = $(SRCS:.cc=.o)
DEPS = $(SRCS:.cc=.d)

# DEPS = $(patsubst %.cc, %.d, $(SRCS) $(TEST_SRCS))


TESTS = \
	auc_test \
	sparse_matrix_test \
	io_test \
	bitmap_test \
	sparse_matrix_perf \
	shared_array_test \
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

all: dir $(addprefix $(BIN)/, ps_mpi text2bin) # ps ps_local
test: $(addprefix $(BIN)/, $(TESTS))

dir:
	mkdir -p $(LIB)
	mkdir -p $(BIN)
clean:
	rm -rf */*.o */*.d proto/*.cc $(BIN) $(LIB)

%.o: %.cc %.h
	$(CC) $(CFLAGS) -c $< -o $@

%.d: %.cc %.h
	$(CC) $(CFLAGS) -MM -MT $*.o $< > $@

$(BIN)/%: test/%.cc $(LIB)/libps.a
	$(CC) $(CFLAGS) $< $(GTEST) $(LDFLAGS)  -o $@

./proto/%.pb.cc ./proto/%.pb.h : ./proto/%.proto
	${THIRD_BIN}/protoc --cpp_out=. $<

-include $(DEPS)

# static link, works fine for both mac and linux.
$(LIB)/libps.a: $(OBJS) #gtest
	ar crv $@ $(OBJS)

# -Wl,-rpath=../lib $(LIB)/libps.so
#  $(LIB)/libps.a

# $(BIN)/ps: ps.cc $(LIB)/libps.so
# 	$(CC) $(CFLAGS) ps.cc $(LDFLAGS) -Wl,-rpath=../lib  -o $@

$(BIN)/ps_mpi: ps_mpi.cc $(LIB)/libps.a
	$(MPICC) $(CFLAGS) ps_mpi.cc $(LDFLAGS) -o $@

$(BIN)/text2bin: data/text2bin.cc data/text2bin.h $(LIB)/libps.a
	$(CC) $(CFLAGS) data/text2bin.cc $(LDFLAGS)  -o $@

# test: $(TESTS)
# 	for t in $(TESTS); do echo "***** Running $$t"; ./$$t || exit 1; done
