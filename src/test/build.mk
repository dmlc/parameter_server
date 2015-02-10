TESTFLAGS= -lgtest_main -lgtest $(LDFLAGS)

build/bitmap_test: src/test/bitmap_test.cc src/util/bitmap.h
	$(CC) $(CFLAGS) $< $(TESTFLAGS) -o $@
