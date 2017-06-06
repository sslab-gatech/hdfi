# Makefile for RIPE
# @author John Wilander & Nick Nikiforakis
LEVEL = ..
SRCS = ./source/ripe_attack_generator.c
DEPS = ./source/ripe_attack_generator.h ./source/ripe_attack_parameters.h
TARGET = ./build/ripe_attack_generator
CFLAGS = -fno-stack-protector
include $(LEVEL)/Makefile.inc

#CFLAGS += -Wall -Werror

clean::
	@rm -f ./build/*

# ATTACK GENERATOR COMPILE
./build/ripe_attack_generator: $(ASMS)
	$(CC) $(CFLAGS) -o $@ $^

