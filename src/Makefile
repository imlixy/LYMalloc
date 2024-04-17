CC = gcc
CFLAGS = -g -Wall -std=c99
OMPFLAGS = -fopenmp
LIBS = -lm -pthread
OUT = allocator


SOURCES = main.c benchmark.c LYMalloc.c
HEADERS = benchmark.h LYMalloc.h  
OBJECTS = $(SOURCES:.c=.o)

all: $(OUT)

%.o: %.c $(HEADERS)
	$(CC) -c $(CFLAGS) $(OMPFLAGS) $< -o $@

$(OUT): $(OBJECTS)
	$(CC) $(CFLAGS) $(OMPFLAGS) $(OBJECTS) -o $@ $(LIBS)

clean:
	rm -f $(OUT) $(OBJECTS)

.PHONY: all clean
