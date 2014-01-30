PREFIX ?= /usr/local
BINDIR = ${PREFIX}/bin

SRC = chaino.c strmap.c
OBJ = ${SRC:.c=.o}

all: chaino

.c.o:
	${CC} -c ${CFLAGS} $<

chaino: ${OBJ}
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

install-dirs:
	install -d ${DESTDIR}${BINDIR}

install: all install-dirs
	install -m 0755 ${NAME} ${DESTDIR}${BINDIR}

uninstall:
	rm -f ${DESTDIR}${BINDIR}/${NAME}

clean:
	rm -f chaino ${OBJ}

.PHONY: all install install-dirs uninstall