all :
	make -C src -j8
test :
	make -C src test -j8
t :
	make -C src t
clean :
	make -C src clean
data :
	make -C data
