.PHONY: all clear

VER_MAJ := 1
VER_MIN := 0

BREW_PREFIX := $(shell brew --prefix)

C = gcc
CFLAGS = -Isrc/include -I$(BREW_PREFIX)/include
LDFLAGS = -L$(BREW_PREFIX)/lib
SRC = src/main.c src/window.c src/input.c src/commands.c src/processes.c
OUT = bin/serve

mac:
	clear
	${MAKE} compileMac

linux:
	clear
	${MAKE} compileLinux

compileMac: ${SRC}
	${C} ${CFLAGS} -DMAC ${SRC} -o ${OUT} ${LDFLAGS} -DVER_MAJ=${VER_MAJ} -DVER_MIN=${VER_MIN}
	chmod +x ${OUT}

compileLinux: ${SRC}
	${C} ${CFLAGS} -DLINUX ${SRC} -o ${OUT} ${LDFLAGS} -DVER_MAJ=${VER_MAJ} -DVER_MIN=${VER_MIN}
	chmod +x ${OUT}

clear:
	rm -rf bin/