.PHONY: cl

VER_MAJ := 1
VER_MIN := 0

C = gcc
CFLAGS = -Isrc/include
SRC = src/main.c src/window.c src/input.c src/commands.c src/processes.c
OUT = bin/serve

mac:
	cl
	${MAKE} compileMac

linux:
	cl
	${MAKE} compileLinux

compileMac: ${SRC}
	${C} ${CFLAGS} -DMAC ${SRC} -o ${OUT} -DVER_MAJ=${VER_MAJ} -DVER_MIN=${VER_MIN}
	chmod +x ${OUT}

compileLinux: ${SRC}
	${C} ${CFLAGS} -DLINUX ${SRC} -o ${OUT}  -DVER_MAJ=${VER_MAJ} -DVER_MIN=${VER_MIN}
	chmod +x ${OUT}

cl:
	rm -rf bin/