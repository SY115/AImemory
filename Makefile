CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11
TARGET = aimemory
SERVER = aimemory-server

all: $(TARGET) $(SERVER)

$(TARGET): src/engine.c src/main.c src/aimemory.h
	$(CC) $(CFLAGS) -o $(TARGET) src/engine.c src/main.c

$(SERVER): src/engine.c src/server.c src/aimemory.h
	$(CC) $(CFLAGS) -o $(SERVER) src/engine.c src/server.c

clean:
	rm -f $(TARGET) $(SERVER)
	rm -rf data/

run: $(TARGET)
	./$(TARGET)

serve: $(SERVER)
	./$(SERVER)

.PHONY: all clean run serve
