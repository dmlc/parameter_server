third_party_dir=$(cd ..;pwd)/third_party
third_party_src=${third_party_dir}/src
third_party_include=${third_party_dir}/include
third_party_library=${third_party_dir}/lib

third_party_all: gflags glog gtest protobuf zeromq

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
	make ;\
	# no make install here, require to add the include and lib path

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






