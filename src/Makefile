CC = g++

# OPT = -O3
OPT = -O0 -ggdb

#-Wconversion -fPIC
WARN = -Wall -Wno-unused-function -finline-functions
INCPATH = -I. -I/usr/local/include \
	-I../third_party/gtest-1.7.0/include \
	-I../third_party
CFLAGS = -std=c++0x $(WARN) $(OPT) $(INCPATH)

LDFLAGS = -L/usr/local/lib libps.a -lgflags -lpthread -lzmq -lprotobuf -lglog
#GTEST = ../third_party/gtest-1.7.0/libgtest_main.a \
#	../third_party/gtest-1.7.0/libgtest.a
GTEST = -L../third_party/gtest-1.7.0 -lgtest_main -lgtest
#GTEST = ../third_party/gtest/libgtest_main.a

OBJECTS = \
	./proto/nodemgt.pb.o \
	./proto/header.pb.o \
	./util/crc32c.o \
	./util/key.o \
	./util/mail.o \
	./util/join.o \
	./util/stringpiece.o \
	./util/stringprintf.o \
	./util/futurepool.o \
	./util/md5.o \
	./util/hashfunc.o \
	./system/van.o \
	./system/node.o \
	./system/node_group.o \
	./system/workload.o \
	./system/postmaster.o \
	./system/postoffice.o \
	./system/hashring.o \
	./system/dht.o \
	./system/replica_manager.o \
	./system/aggregator.o \
	./box/consistency.o \
	./box/item.o \
	./box/container.o \
	./box/sparse_vector.o \
	./algo/inference.o \
	./algo/graddesc.o
#./box/vectors.o

DEPS = \
	util/rawarray.h


TESTS = \
	vectors_test \
	van_test \
	fault_tolerance_press \
	postmaster_test \
#	replica_test \
	key_test \
	xarray_test \
	eigen3_test \
	vector_test \
	item_test \
	densevector_test \
	sparse_vector_test \
#	item_test \
	consistency_test \
	workload_test \
	aggregator_test \
	common_test

# PWD  := $(shell pwd)
#r: clean all
all: $(TESTS) ps gtest

clean:
	rm -f */*.o *.a ps

test: # $(TESTS)
	for t in $(TESTS); do echo "***** Running $$t"; ./$$t || exit 1; done

%.o: %.cc %.h
	$(CC) $(CFLAGS) -c $< -o $@

libps.a: $(OBJECTS) $(DEPS) gtest
	ar crv libps.a $(OBJECTS)

./proto/%.pb.cc ./proto/%.pb.h : ./proto/%.proto
	protoc --cpp_out=. $<

#base:
#	make -C $(PWD)/../third_party/ #M=$(PWD)

gtest:
	cmake ../third_party/gtest-1.7.0/CMakeLists.txt
	make -C ../third_party/gtest-1.7.0/

ps: libps.a ps.cc
	$(CC) $(CFLAGS) ps.cc $(LDFLAGS) -o parse

fault_tolerance_press: algo/fault_tolerance_press.cc libps.a algo/fault_tolerance_press.cc
	$(CC) $(CFLAGS) algo/fault_tolerance_press.cc $(LDFLAGS) -o fault_tolerance_press

common_test : util/common_test.cc
	$(CC) $(CFLAGS) util/common_test.cc $(GTEST) $(LDFLAGS) -o $@

hashfunc_test: util/hashfunc_test.cc
	$(CC) $(CFLAGS) util/hashfunc_test.cc $(GTEST) $(LDFLAGS) -o $@

hashring_test: system/hashring_test.cc
	$(CC) $(CFLAGS) system/hashring_test.cc $(GTEST) $(LDFLAGS) -o $@

metadata_test : box/metadata_test.cc
	$(CC) $(CFLAGS) box/metadata_test.cc $(GTEST) $(LDFLAGS) -o $@

workload_test : system/workload_test.cc libps.a
	$(CC) $(CFLAGS) system/workload_test.cc $(GTEST) $(LDFLAGS) -o $@

van_test : system/van_test.cc libps.a
	$(CC) $(CFLAGS) system/van_test.cc $(GTEST) $(LDFLAGS) -o $@

consistency_test : box/consistency_test.cc libps.a
	$(CC) $(CFLAGS) box/consistency_test.cc $(GTEST) $(LDFLAGS) -o $@

aggregator_test : box/aggregator_test.cc libps.a
	$(CC) $(CFLAGS) box/aggregator_test.cc $(GTEST) $(LDFLAGS) -o $@

item_test : box/item_test.cc libps.a
	$(CC) $(CFLAGS) box/item_test.cc $(GTEST) $(LDFLAGS) -o $@

key_test : util/key_test.cc libps.a
	$(CC) $(CFLAGS) util/key_test.cc $(GTEST) $(LDFLAGS) -o $@

xarray_test : util/xarray_test.cc util/xarray.h
	$(CC) $(CFLAGS) util/xarray_test.cc $(GTEST) $(LDFLAGS) -o $@

vectors_test : box/vectors_test.cc libps.a
	$(CC) $(CFLAGS) box/vectors_test.cc  $(GTEST) $(LDFLAGS) -o $@

eigen3_test : util/eigen3_test.cc libps.a
	$(CC) $(CFLAGS) util/eigen3_test.cc  $(GTEST) $(LDFLAGS) -o $@

sparsevector_test :  libps.a
	$(CC) $(CFLAGS) box/sparsevector.cc  $(GTEST) $(LDFLAGS) -o $@

sparse_vector_test :  box/sparse_vector_test.cc libps.a
	$(CC) $(CFLAGS) box/sparse_vector_test.cc  $(GTEST) $(LDFLAGS) -o $@

postmaster_test : system/postmaster_test.cc system/postmaster.cc system/postmaster.h libps.a
	$(CC) $(CFLAGS) system/postmaster_test.cc  $(GTEST) $(LDFLAGS) -o $@

replica_test : system/replica_test.cc libps.a
	$(CC) $(CFLAGS) system/replica_test.cc $(GTEST) $(LDFLAGS) -o $@
