# tagger_test benchmark that uses the following as a skeletone
#=======================================================================
# UCB CS250 Makefile fragment for benchmarks
#-----------------------------------------------------------------------
#
# Each benchmark directory should have its own fragment which
# essentially lists what the source files are and how to link them
# into an riscv and/or host executable. All variables should include
# the benchmark name as a prefix so that they are unique.
#

tagger_test_c_src = \
	tagger-test_main.c \
	syscalls.c \

tagger_test_riscv_src = \
	crt.S \

tagger_test_c_objs     = $(patsubst %.c, %.o, $(tagger_test_c_src))
tagger_test_riscv_objs = $(patsubst %.S, %.o, $(tagger_test_riscv_src))

tagger_test_host_bin = tagger-test.host
$(tagger_test_host_bin) : $(tagger_test_c_src)
	$(HOST_COMP) $^ -o $(tagger_test_host_bin)

tagger_test_riscv_bin = tagger-test.riscv
$(tagger_test_riscv_bin) : $(tagger_test_c_objs) $(tagger_test_riscv_objs)
	$(RISCV_LINK) $(tagger_test_c_objs) $(tagger_test_riscv_objs) -o $(tagger_test_riscv_bin) $(RISCV_LINK_OPTS)

junk += $(tagger_test_c_objs) $(tagger_test_riscv_objs) \
        $(tagger_test_host_bin) $(tagger_test_riscv_bin)
