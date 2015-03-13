# build binary for the tutorial

build/ping: src/app/tutorial/ping.cc $(PS_LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@
