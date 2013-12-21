CC = g++

# OPT = -O3
OPT = -O0 -ggdb #-rdynamic

#-Wconversion -fPIC
WARN = -Wall -Wno-unused-function -finline-functions -Wno-sign-compare
INCPATH = -I. -I/usr/local/include -I${third_party_include}
CFLAGS = -std=c++0x $(WARN) $(OPT) $(INCPATH)

LDFLAGS = -Wl,-rpath,/usr/local/lib -Wl,-rpath,${third_party_library}\
	 -L/usr/local/lib -L${third_party_library}\
	 libps.a -lgflags -lpthread -lzmq -lprotobuf -lglog
GTEST =  -lgtest_main -lgtest

OBJECTS = \
	./proto/nodemgt.pb.o \
	./proto/header.pb.o \
	./util/crc32c.o \
	./util/key.o \
	./util/mail.o \
	./util/join.o \
	./util/logging.o \
	./util/stringpiece.o \
	./util/stringprintf.o \
	./util/MurmurHash3.o \
	./util/md5.o \
	./util/hashfunc.o \
	./util/BaseConverter.o\
	./system/van.o \
	./system/node.o \
	./system/node_group.o \
	./system/workload.o \
	./system/postmaster.o \
	./system/postoffice.o \
	./system/hashring.o \
	./system/dht.o \
	./system/replica_manager.o \
	./box/item.o \
	./box/container.o \
	./box/sparse_vector.o \
	./algo/inference.o \
	./algo/graddesc.o

DEPS = \
	./util/rawarray.h \
	./box/vectors.h \
	./box/vectors-inl.h \
	./system/aggregator.h \
	./box/consistency.h \
	./util/futurepool.h


TESTS = \
	vectors_test \
#	van_test \
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

all: $(TESTS) # ps

clean:
	rm -f */*.o *.a ps

test: # $(TESTS)
	for t in $(TESTS); do echo "***** Running $$t"; ./$$t || exit 1; done

%.o: %.cc %.h
	$(CC) $(CFLAGS) -c $< -o $@

libps.a: $(OBJECTS) $(DEPS) #gtest
	ar crv libps.a $(OBJECTS)

./proto/%.pb.cc ./proto/%.pb.h : ./proto/%.proto
#	${third_party_bin}/protoc --cpp_out=. $<
	protoc --cpp_out=. $<

#base:
#	make -C $(PWD)/../third_party/ #M=$(PWD)

#gtest:
#	cmake ../third_party/gtest-1.7.0/CMakeLists.txt
#	make -C ../third_party/gtest-1.7.0/

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
	$(CC) $(CFLAGS) box/vectors_test.cc  -lgtest $(LDFLAGS) -o $@

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

####################################
# install third_party

third_party_dir=$(shell cd ..;pwd)/third_party
third_party_src=${third_party_dir}/src
third_party_include=${third_party_dir}/include
third_party_library=${third_party_dir}/lib
third_party_bin=${third_party_dir}/bin

third_party_all: gflags glog gtest protobuf zeromq eigen

gflags:
	wget -r --no-check-certificate -nc https://github.com/zcyang/PS_thirdparty/raw/master/gflags-2.0-no-svn-files.tar.gz -nd -P ${third_party_src}
	cd ${third_party_src} ;\
	pwd ;\
	tar xzvf ${third_party_src}/gflags-2.0-no-svn-files.tar.gz ;\
	cd ${third_party_src}/gflags-2.0 ;\
	./configure --prefix=${third_party_dir} ;\
	make ;\
	make install

glog:
	wget -r --no-check-certificate -nc https://github.com/zcyang/PS_thirdparty/raw/master/glog-0.3.3.tar.gz -nd -P ${third_party_src}
	cd ${third_party_src} ;\
	pwd ;\
	tar xzvf ${third_party_src}/glog-0.3.3.tar.gz ;\
	cd ${third_party_src}/glog-0.3.3 ;\
	./configure --prefix=${third_party_dir} ;\
	make ;\
	make install

gtest:
	wget -r --no-check-certificate -nc https://github.com/zcyang/PS_thirdparty/raw/master/gtest-1.7.0.zip -nd -P ${third_party_src}
	cd ${third_party_src} ;\
	pwd ;\
	unzip ${third_party_src}/gtest-1.7.0.zip ;\
	cd ${third_party_src}/gtest-1.7.0 ;\
	./configure --prefix=${third_party_dir} ;\
	cmake CMakeLists.txt ;\
	make ;\
	cp -r include/gtest ${third_party_include}; \
	cp libgtest.a  libgtest_main.a ${third_party_library}

protobuf:
	wget -r --no-check-certificate -nc https://github.com/zcyang/PS_thirdparty/raw/master/protobuf-2.5.0.tar.gz -nd -P ${third_party_src}
	cd ${third_party_src} ;\
	pwd ;\
	tar xzvf ${third_party_src}/protobuf-2.5.0.tar.gz ;\
	cd ${third_party_src}/protobuf-2.5.0 ;\
	./configure --prefix=${third_party_dir} ;\
	make ;\
	make install

zeromq:
	wget -r --no-check-certificate -nc https://github.com/zcyang/PS_thirdparty/raw/master/zeromq-4.0.1.tar.gz -nd -P ${third_party_src}
	cd ${third_party_src} ;\
	pwd ;\
	tar xzvf ${third_party_src}/zeromq-4.0.1.tar.gz ;\
	cd ${third_party_src}/zeromq-4.0.1 ;\
	./configure --prefix=${third_party_dir} ;\
	make ;\
	make install

eigen:
	wget -r --no-check-certificate -nc https://github.com/zcyang/PS_thirdparty/raw/master/3.2.0.tar.gz -nd -P ${third_party_src}
	cd ${third_party_src} ;\
	pwd ;\
	tar xzvf ${third_party_src}/3.2.0.tar.gz ;\
	cd ${third_party_src}/eigen-eigen-ffa86ffb5570 ;\
	mkdir ${third_party_include}/eigen3 ;\
	cp -r Eigen/ ${third_party_include}/eigen3
