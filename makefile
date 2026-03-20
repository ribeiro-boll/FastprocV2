CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lncursesw -lpthread
TARGET = fastproc
SRC = main.c
DESKTOP_FILE = fastproc.desktop
DESKTOP_PATH = /usr/share/applications

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

install:
	cp $(TARGET) /usr/local/bin
	cp $(DESKTOP_FILE) $(DESKTOP_PATH)/
	update-desktop-database

remove:
	rm -f /usr/local/bin/$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all install remove clean
