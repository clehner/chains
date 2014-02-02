PREFIX ?= /usr/local
BINDIR = ${PREFIX}/bin

BIN = chaino
DEPS = $(wildcard deps/*/*.c)
SRC = chaino.c ${DEPS}
OBJ = $(SRC:.c=.o)
CFLAGS = -std=c99 -Ideps -Wall
LDFLAGS =

all: $(BIN)

$(BIN): $(OBJ)
	${CC} -o $@ ${OBJ} ${LDFLAGS}

.c.o:
	${CC} -c ${CFLAGS} $< -o $@

install: all
	install -m 0755 ${BIN} ${DESTDIR}${BINDIR}

uninstall:
	rm -f ${DESTDIR}${BINDIR}/${BIN}

clean:
	rm -f $(BIN)

.PHONY: all install uninstall
