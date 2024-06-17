MODE = release

include config.mk
export CFLAGS LDFLAGS

.PHONY: all
all: httpd

httpd: httpd.o threadpool.o
	gcc $(LDFLAGS) -o $@ $^

%.o: %.c
	gcc $(CFLAGS) -c $<

.PHONY: json
json: clean
	bear -- make

.PHONY: clean
clean:
	-rm -f *.o httpd
