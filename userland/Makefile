CC = musl-gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
LDFLAGS = -static -lm

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)
TARGET = init

.PHONY: all clean

all: $(TARGET)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

clean:
	rm -f src/*.o $(TARGET)
