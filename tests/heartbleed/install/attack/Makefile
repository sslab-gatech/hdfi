CROSS_COMPILE=riscv64-unknown-linux-gnu-
CC=$(CROSS_COMPILE)gcc
TARGET=attack
SRC=$(TARGET).c

all: $(TARGET)

$(TARGET):$(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf $(TARGET)
