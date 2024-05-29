CFLAGS=-std=c11 -g -lcurl -fno-common -Wall -Wno-switch
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

TEST_SRCS=$(wildcard test/*.c)
TESTS=$(TEST_SRCS:.c=.exe)

PREFIX=/usr/local
BINDIR=$(PREFIX)/bin/xcc

# Stage 1
xcc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJS): xcc.h

test/%.exe: xcc test/%.c
	./xcc -Iinclude -Itest -c -o test/$*.o test/$*.c
	$(CC) -pthread -o $@ test/$*.o -xc test/common

test: $(TESTS)
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done
	test/driver.sh ./xcc

test-all: test test-stage2

# Stage 2
stage2/xcc: $(OBJS:%=stage2/%)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

stage2/%.o: xcc %.c
	mkdir -p stage2/test
	./xcc -c -o $(@D)/$*.o $*.c

stage2/test/%.exe: stage2/xcc test/%.c
	mkdir -p stage2/test
	./stage2/xcc -Iinclude -Itest -c -o stage2/test/$*.o test/$*.c
	$(CC) -pthread -o $@ stage2/test/$*.o -xc test/common

test-stage2: $(TESTS:test/%=stage2/test/%)
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done
	test/driver.sh ./stage2/xcc

# Stage 3
.SILENT: uninstall
install: xcc 
	@if [ ! -d $(BINDIR) ]; then \
		install -d $(BINDIR); \
	fi
	install -m 755 xcc $(BINDIR)
	sudo install -m 755 ./xc /usr/local/bin/
	@if [ ! -d /usr/local/include/xcc/ ]; then \
		sudo mkdir -p /usr/local/include/xcc/; \
	fi
	@sudo chmod +x xc
	@sudo cp -r ./include/* /usr/local/include/xcc/
	@rm -rf xcc tmp* $(TESTS) test/*.s test/*.exe stage2
	@find * -type f '(' -name '*~' -o -name '*.o' ')' -exec rm {} ';'

.SILENT: clean
uninstall:
	sudo rm -rf /usr/local/bin/xcc /usr/local/bin/xc /usr/local/include/xcc/

clean:
	rm -rf xcc tmp* $(TESTS) test/*.s test/*.exe stage2
	find * -type f '(' -name '*~' -o -name '*.o' ')' -exec rm {} ';'

.PHONY: test clean test-stage2 install uninstall
