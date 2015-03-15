TESTFLAGS= -lgtest_main -lgtest $(LDFLAGS)

build/hello_test: src/test/hello_test.cc $(PS_LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

build/aggregation_test: src/test/aggregation_test.cc $(PS_LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

build/network_perf: src/test/network_perf.cc $(PS_LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

build/common_test: src/test/common_test.cc src/util/common.h
	$(CC) $(CFLAGS) $< $(TESTFLAGS) -o $@

# build/bitmap_test: src/test/bitmap_test.cc src/util/bitmap.h
# 	$(CC) $(CFLAGS) $< $(TESTFLAGS) -o $@

# build/reassign_server_key_range: src/test/reassign_server_key_range.cc $(PS_LIB)
# 	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# build/fixing_float_test: src/test/fixing_float_test.cc src/filter/fixing_float.h $(PS_LIB)
# 	$(CC) $(CFLAGS) $< $(PS_LIB) $(TESTFLAGS) -o $@
