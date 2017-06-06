LEVEL = ..
SRCS = main.c
TARGET = main
include $(LEVEL)/Makefile.inc

$(TARGET):
	./compile.py $@
	$(SPIKE) pk $@

clean::
	@rm -rf $(TARGET)
