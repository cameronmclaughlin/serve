VER_MAJ := 1
VER_MIN := 0

C = gcc
CFLAGS = -Isrc/include
SRC = src/main.c src/window.c src/input.c src/commands.c src/processes.c
OUT = bin/serve

mac: ${SRC}
	${C} ${CFLAGS} -DMAC ${SRC} -o ${OUT} -DVER_MAJ=${VER_MAJ} -DVER_MIN=${VER_MIN}
	chmod +x ${OUT}

linux: ${SRC}
	${C} ${CFLAGS} -g -DLINUX ${SRC} -o ${OUT}  -DVER_MAJ=${VER_MAJ} -DVER_MIN=${VER_MIN}
	chmod +x ${OUT}