CC = gcc
CFLAGS = -Wall -std=c99
OMPFLAGS = -fopenmp
OUT = allocator

SOURCES = main.c LYMalloc.c
HEADERS = LYMalloc.h
OBJECTS = $(SOURCES:.c=.o)

all: $(OUT)

%.o: %.c $(HEADERS)
	$(CC) -c $(CFLAGS) $(OMPFLAGS) $< -o $@

$(OUT): $(OBJECTS)
	$(CC) $(CFLAGS) $(OMPFLAGS) $(OBJECTS) -o $@

clean:
	rm -f $(OUT) $(OBJECTS)

.PHONY: all clean

