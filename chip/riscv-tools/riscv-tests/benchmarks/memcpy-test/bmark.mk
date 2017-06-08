#=======================================================================
# UCB CS250 Makefile fragment for benchmarks
#-----------------------------------------------------------------------
#
# Each benchmark directory should have its own fragment which
# essentially lists what the source files are and how to link them
# into an riscv and/or host executable. All variables should include
# the benchmark name as a prefix so that they are unique.
#

memcpy_test_c_src = \
	memcpy-test_main.c \
	syscalls.c \

memcpy_test_riscv_src = \
	crt.S \

memcpy_test_c_objs     = $(patsubst %.c, %.o, $(memcpy_test_c_src))
memcpy_test_riscv_objs = $(patsubst %.S, %.o, $(memcpy_test_riscv_src))

memcpy_test_host_bin = memcpy-test.host
$(memcpy_test_host_bin): $(memcpy_test_c_src)
	$(HOST_COMP) $^ -o $(memcpy_test_host_bin)

memcpy_test_riscv_bin = memcpy-test.riscv
$(memcpy_test_riscv_bin): $(memcpy_test_c_objs) $(memcpy_test_riscv_objs)
	$(RISCV_LINK) $(memcpy_test_c_objs) $(memcpy_test_riscv_objs) \
    -o $(memcpy_test_riscv_bin) $(RISCV_LINK_OPTS)

junk += $(memcpy_test_c_objs) $(memcpy_test_riscv_objs) \
        $(memcpy_test_host_bin) $(memcpy_test_riscv_bin)
