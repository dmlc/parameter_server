guide: \
guide/example_a

guide/%: guide/%.cc $(PS_LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@
