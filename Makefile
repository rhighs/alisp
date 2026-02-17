PROG := alisp
CC   := gcc
SRC  := mpc.c main.c alisp.c

CFLAGS_DEBUG   := -g -O0 -Wall -Wextra -std=c17 -DDEBUG
CFLAGS_RELEASE := -O3 -DNDEBUG -Wall -Wextra -std=c17

LDFLAGS := -ledit

all: debug

builddir:
	mkdir -p build/bin

debug: builddir
	$(CC) $(SRC) -o build/bin/$(PROG)-dev $(CFLAGS_DEBUG) $(LDFLAGS)

release: builddir
	$(CC) $(SRC) -o build/bin/$(PROG) $(CFLAGS_RELEASE) $(LDFLAGS)

clean:
	rm -rf $(PROG) build
