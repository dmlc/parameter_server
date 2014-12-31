all :
	make -C src -j8
test :
	make -C src test -j8
clean :
	make -C src clean
