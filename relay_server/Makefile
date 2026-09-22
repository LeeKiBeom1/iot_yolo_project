CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -pthread
LIBS = -lsqlite3 -lcjson -pthread

TARGET = relay_server
SRC = $(wildcard src/*.c)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)
