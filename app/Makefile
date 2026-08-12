CC = musl-gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -static -lm

SRC = calc.c
ASM = calc.s
OBJ = calc.o
TARGET = calc

.PHONY: all clean

all: $(TARGET)

$(ASM): $(SRC)
	$(CC) $(CFLAGS) -S $< -o $@

$(OBJ): $(ASM)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

clean:
	rm -f $(ASM) $(OBJ) $(TARGET)
