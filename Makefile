EXES := dfo-i2c

all: $(EXES)

libcintelhex/src/.libs/libcintelhex.a: libcintelhex/Makefile
	cd libcintelhex && make

libcintelhex/Makefile: libcintelhex/configure libcintelhex/Makefile.am
	cd libcintelhex && ./configure

libcintelhex/configure: libcintelhex/configure.ac
	cd libcintelhex && autoreconf -i

dfo-i2c: dfo-i2c.c libcintelhex/src/.libs/libcintelhex.a
	$(CC) -Wall -std=c99 -O2 dfo-i2c.c libcintelhex/src/.libs/libcintelhex.a -o dfo-i2c

.PHONY: clean
clean:
	rm -f *.o $(EXES)
