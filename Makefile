all: libmailer.a journalmon

libmailer.a:
	@cd libmailer && make

journalmon: libmailer.a
	@mkdir -p ./bin
	gcc -Wall -Wextra -static -O3 \
		-o ./bin/journalmon ./journalmon.c ./libmailer/libmailer.a \
		-pthread -ldl
