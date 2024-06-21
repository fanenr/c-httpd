MODE = debug

include config.mk
export CFLAGS LDFLAGS

.PHONY: all
all: test

test: test.o mstr.o httpd.o rbtree.o threadpool.o
	gcc $(LDFLAGS) -o $@ $^

%.o: %.c
	gcc $(CFLAGS) -c $<

.PHONY: json
json: clean
	bear -- make

.PHONY: clean
clean:
	-rm -f *.o test
