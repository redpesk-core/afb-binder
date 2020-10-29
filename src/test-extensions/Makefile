all: logo.so tone.so

.SUFFIXES: .so

%.so: %.c
	gcc -shared -o $@ $< -I.. -fPIC -lafb
