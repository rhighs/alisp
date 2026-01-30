PROG := alisp
CC   := gcc
SRC  := mpc.c main.c alisp.c

CFLAGS_DEBUG   := -g -O0 -Wall -Wextra -std=c17 -DDEBUG
CFLAGS_RELEASE := -O3 -DNDEBUG -Wall -Wextra -std=c17

LDFLAGS := -ledit

all: debug

debug:
	$(CC) $(SRC) -o $(PROG) $(CFLAGS_DEBUG) $(LDFLAGS)

release:
	$(CC) $(SRC) -o $(PROG) $(CFLAGS_RELEASE) $(LDFLAGS)

clean:
	rm -f $(PROG)
