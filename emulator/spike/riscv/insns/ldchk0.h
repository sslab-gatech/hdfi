require_rv64;
WRITE_RD(MMU.load_int64_with_tag(RS1 + insn.i_imm(), 0));
