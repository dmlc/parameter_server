TESTFLAGS= -lgtest_main -lgtest $(LDFLAGS)

build/bitmap_test: src/test/bitmap_test.cc src/util/bitmap.h
	$(CC) $(CFLAGS) $< $(TESTFLAGS) -o $@

build/reassign_server_key_range: src/test/reassign_server_key_range.cc $(PS_LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@
