CC = gcc
CFLAGS = -Wall -Wextra -Iinclude $(shell mariadb_config --cflags)
LIBS = $(shell mariadb_config --libs) -lcjson

TARGET = ubuntu_server
SRC = $(wildcard src/*.c)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)

