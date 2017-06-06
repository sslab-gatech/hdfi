import struct
import subprocess as sp

def p64(s):
    return struct.pack('<Q', s)

def nm(exe, symbol):
    nm_b = "../../install/bin/riscv64-unknown-linux-gnu-nm"
    cmd = ["../../install/bin/riscv64-unknown-linux-gnu-nm", exe]
    p = sp.Popen(cmd, stdout = sp.PIPE)
    for line in p.stdout:
        line = line.decode("utf-8").strip()
        if line.endswith(" %s" % symbol):
            return int(line.split(' ')[0],16)
    raise("Cannot find symbol %s" % symbol)
