TESTFLAGS= -lgtest_main -lgtest $(LDFLAGS)

build/bitmap_test: src/test/bitmap_test.cc src/util/bitmap.h
	$(CC) $(CFLAGS) $< $(TESTFLAGS) -o $@

build/reassign_server_key_range: src/test/reassign_server_key_range.cc $(PS_LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

build/fixing_float_test: src/test/fixing_float_test.cc src/filter/fixing_float.h $(PS_LIB)
	$(CC) $(CFLAGS) $< $(PS_LIB) $(TESTFLAGS) -o $@
