# Makefile wrapper for waf

all:
	./waf

# free to change this part to suit your requirements
configure:
	./waf configure -d debug --enable-examples --enable-tests --disable-werror

build:
	./waf build

install:
	./waf install

clean:
	./waf clean

distclean:
	./waf distclean
