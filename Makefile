CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lm
TARGET = lab9_app

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c $(LDFLAGS)

clean:
	rm -f $(TARGET)
