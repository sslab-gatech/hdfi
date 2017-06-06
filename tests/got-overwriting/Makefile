LEVEL = ..
SRCS = main.c
TARGET = main
CROSS_COMPILE=riscv64-unknown-linux-gnu-
include $(LEVEL)/Makefile.inc
PYTHONPATH=../shared/
CFLAGS=-pie -fPIC

$(TARGET):$(SRCS)
	$(CC) $(CFLAGS) -o $@ $<
	PYTHONPATH=../shared ./gen-payload.py $(TARGET)

cps:$(SRCS)
	$(CLANG) $(CFLAGS) -target riscv -mriscv=RV64IAMFD -S -o $@.s $<
	$(CC) -o $@ $@.s
	PYTHONPATH=../shared ./gen-payload.py cps

clean::
	@rm -rf $(TARGET) cps

