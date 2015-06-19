SRC    = $(shell find src/ -name '*.c')
BIN    = bin/httpserv
CFLAGS = -Wall -Wextra -Werror -Wfatal-errors -std=c99 -pedantic
LFLAGS = -lpthread


.SILENT:


debug: CFLAGS += -DDEBUG=1 -ggdb
debug: build

clean:
	rm -rf bin/ *.core

run:
	./$(BIN)


build:
	mkdir -p $(dir $(BIN))
	$(CC) -o $(BIN) $(CFLAGS) $(LFLAGS) $(SRC)

