LEVEL = ..
SRCS = main.c
TARGET = main
CROSS_COMPILE=riscv64-unknown-linux-gnu-
include $(LEVEL)/Makefile.inc
PYTHONPATH=../shared/
CFLAGS=-pie -fPIC -I../shared

$(TARGET):$(SRCS)
	$(CC) $(CFLAGS) -o $@ $<

clean::
	@rm -rf $(TARGET) cps

