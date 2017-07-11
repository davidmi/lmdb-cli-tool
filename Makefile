CC=gcc

CC_OPT=-Wall -Wextra -Werror -std=c99
CFLAGS := -llmdb

CHECK_CFLAGS := $(shell pkg-config --cflags --libs check)

prefix    = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
mandir = $(prefix)/share/man/man1

mdbx: mdbx.o main.c
	$(CC) $(CC_OPT) -o mdbx mdbx.o main.c -g $(CFLAGS)

mdbx.o: mdbx.c
	$(CC) $(CC_OPT) -c -o mdbx.o mdbx.c -g $(CFLAGS)

test: mdbx.o test.c
	$(CC) $(CC_OPT) -o test mdbx.o test.c -g -llmdb $(CHECK_CFLAGS)

install: mdbx
	mkdir -p $(DESTDIR)$(bindir)
	cp mdbx $(DESTDIR)$(bindir)
	mkdir -p $(DESTDIR)$(mandir)
	cp mdbx.1 $(DESTDIR)$(mandir)/mdbx.1
