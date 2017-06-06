LEVEL = ..
SRCS = main.c
TARGET = main
PYTHONPATH=../shared/
BIN_DIR=../../install/bin/
CC=$(BIN_DIR)riscv64-unknown-linux-gnu-gcc

$(TARGET):$(SRCS)
	$(CC) $(CFLAGS) -o $@ $<
	PYTHONPATH=../shared ./gen-payload.py $(TARGET)

clean::
	@rm -rf $(TARGET)
