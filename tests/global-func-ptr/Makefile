LEVEL = ..
SRCS = main.c
TARGET = main
CROSS_COMPILE=riscv64-unknown-linux-gnu-
include $(LEVEL)/Makefile.inc
PYTHONPATH=../shared/

$(TARGET):$(SRCS)
	$(CC) $(CFLAGS) -o $@ $<

clean::
	@rm -rf $(TARGET) cps

