CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -I../common -pthread
LIBS = -lsqlite3 -lcjson -pthread

TARGET = relay_server
SRC = $(wildcard src/*.c)

$(TARGET): $(SRC) $(wildcard include/*.h ../common/*.h)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)
