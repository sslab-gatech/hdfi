#!/usr/bin/env python3
import os
import multiprocessing as mp
import shutil
import stat

OPENSSL = 'openssl-1.0.1a'
INSTALL_DIR = "install"
ROOT = os.path.abspath(os.path.dirname(__file__))
INCLUDE_DIR = "-I%s/../shared" % ROOT

def set_build_env():
    path = os.getenv("PATH")
    path += ":%s/../../install/bin" % ROOT
    print(path)
    os.putenv("PATH", path)

def build(cflags, dst):
    os.chdir(OPENSSL)
    os.system("make clean")
    os.system("./Configure -DRISCV %s linux-generic64 --cross-compile-prefix=riscv64-unknown-linux-gnu- --prefix=../install %s" % (cflags, INCLUDE_DIR))
    os.system("make")

    os.chdir("../") # back to current
    dst = "%s/" % INSTALL_DIR + dst
    shutil.copyfile("%s/apps/openssl" % OPENSSL, dst)
    os.chmod(dst, os.stat(dst).st_mode | stat.S_IEXEC)

if __name__ == '__main__':
    set_build_env()

    build("-DHEARTBLEED -DHDFI", "openssl_hb_hdfi")
    build("-DHEARTBLEED", "openssl_hb")
    build("-DHDFI", "openssl_hdfi")
