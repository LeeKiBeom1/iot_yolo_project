CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -I../common -pthread $(shell mariadb_config --cflags)
LIBS = $(shell mariadb_config --libs) -lcjson -pthread

TARGET = ubuntu_server
SRC = $(wildcard src/*.c)

$(TARGET): $(SRC) $(wildcard include/*.h ../common/*.h)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)

