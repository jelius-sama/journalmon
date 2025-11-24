all: libmailer/libmailer.a bin/journalmon

CC := gcc
CFLAGS := -Wall -Wextra -static -O3
CSLIBS := -L./libmailer -lmailer

libmailer/libmailer.a:
	@cd libmailer && make

bin/journalmon: libmailer/libmailer.a
	@mkdir -p ./bin
	$(CC) $(CFLAGS) -o ./bin/journalmon ./journalmon.c $(CSLIBS)
