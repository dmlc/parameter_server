test: build/hello_test \
build/aggregation_test \
build/network_perf \
build/assign_op_test \
build/parallel_ordered_match_test \
build/common_test

build/hello_test: src/test/hello_test.cc $(PS_LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

build/aggregation_test: src/test/aggregation_test.cc $(PS_LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

build/network_perf: src/test/network_perf.cc $(PS_LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# google test
TESTFLAGS = $(TEST_MAIN) -lgtest $(LDFLAGS)

build/parallel_ordered_match_test: build/util/file.o build/util/proto/*.o build/data/proto/*.pb.o

build/%_test: build/test/%_test.o
	$(CC) $(CFLAGS) $(filter %.o %.a %.cc, $^) $(TESTFLAGS) -o $@

# build/reassign_server_key_range: src/test/reassign_server_key_range.cc $(PS_LIB)
# 	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# build/fixing_float_test: src/test/fixing_float_test.cc src/filter/fixing_float.h $(PS_LIB)
# 	$(CC) $(CFLAGS) $< $(PS_LIB) $(TESTFLAGS) -o $@
