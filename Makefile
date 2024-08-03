MODE = debug

include config.mk
export CFLAGS LDFLAGS

CFLAGS += -pthread
LDFLAGS += -pthread

.PHONY: all
all: test

test: test.o mstr.o mime.o httpd.o\
      arena.o rbtree.o respool.o threadpool.o
	gcc $(LDFLAGS) -o $@ $^

%.o: %.c
	gcc $(CFLAGS) -c $<

.PHONY: json
json: clean
	bear -- make

.PHONY: clean
clean:
	-rm -f *.o test
