test: build/hello_ps \
build/aggregation_ps \
build/network_perf_ps \
build/kv_vector_ps \
build/kv_vector_buffer_ps \
build/kv_map_ps \
build/kv_map_perf_ps \
build/kv_layer_ps \
build/assign_op_test \
build/parallel_ordered_match_test \
build/common_test

build/%_ps: src/test/%_ps.cc $(PS_LIB)
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
