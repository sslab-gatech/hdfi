#!/usr/bin/env python3
import os
import sys
import subprocess as sp

ROOT = os.path.dirname(__file__)
BIN_DIR = "%s/../../install/bin/" % ROOT
CROSS_COMPILE = "riscv64-unknown-elf-"
OBJDUMP = "%s/%sobjdump" % (BIN_DIR, CROSS_COMPILE)
GCC = "%s/%sgcc" % (BIN_DIR, CROSS_COMPILE)

def skip(n, p):
    for i in range(n):
        p.stdout.readline()

def sh(cmds):
    assert(os.spawnvp(os.P_WAIT, cmds[0], cmds) == 0)

def get_meta(prog):
    # get distance and address of got
    p = sp.Popen([OBJDUMP, "-D", prog], stdout=sp.PIPE)

    for line in p.stdout:
        if b"<__call_exitprocs>:" in line:
            break;

    # skip
    skip(2, p)
    
    # get _global_impure_ptr
    line = p.stdout.readline().decode("utf-8")
    line = line.split("\t")[3] # operands
    addr = line.split("#")[1]

    addr = int(addr.strip().split(" ")[0], 16)
    #print("ADDR : %08X" % addr)

    skip(17, p)

    line = p.stdout.readline().decode("utf-8")

    # get dist
    line = line.split("\t")[3]
    line = line.split(",")[1]
    dist = int(line.split("(")[0])
    #print("DIST : %08X" % dist)
    return (addr, dist)


def build(addr, dist, src, prog):
    cflags = "-O0"
    
    cmds = []
    cmds.append(GCC)
    cmds.append("-DADDR=%du" % addr)
    cmds.append("-DDIST=%du" % dist)
    cmds += ["-O0", "-o", prog, src]
    sh(cmds)

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("usage : %s prog" % sys.argv[0])
        sys.exit(-1)

    prog = sys.argv[1]
    src = "%s.c" % prog

    # build gcc
    build(0x01234578, 0x123, src, prog)
    addr, dist = get_meta(prog)
    build(addr, dist, src, prog)

    assert( (addr,dist) == get_meta(prog) )
