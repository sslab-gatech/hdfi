# Hardware-assisted Data-flow Isolation

Hardware-assisted data-flow isolation (HDFI) is a new fine-grained data
isolation mechanism that is broadly applicable and very efficient.
HDFI enforces isolation at the machine word granularity by virtually extending
each memory unit with an additional tag that is defined by data-flow.
This capability allows HDFI to enforce a variety of security models such as
the Biba Integrity Model and the Bell–LaPadula Model.
The current HDFI prototype is based the RISC-V instruction set architecture (ISA).

This repo contains five parts of the projects:

* chip: the modified [rocket-chip](https://github.com/freechipsproject/rocket-chip)
* emulator: the modified emulator (based on spike)
* toolchain: modified GCC, glibc, and llvm
* linux: the modified kernel
* tests: various tests, mostly for security tests

## More details
* HDFI paper (IEEE S&P'16): http://www.ieee-security.org/TC/SP2016/papers/0824a001.pdf

## Getting started

A test drive with c++ simulator generated from the implementaion in Chisel.

    $ make gcc-build-elf
    $ make fesvr-build
    $ export RISCV=`pwd`/install/
    $ export PATH=$PATH:`pwd`/install/bin/
    $ cd chip/riscv-tools
    $ ./build-tests.sh
    $ cd ../emulator
    $ make all
    $ make run-bmark-tests

For further detail, please follow the RISC-V tutorials:
* hw/README.md
* https://riscv.org/software-tools/
* https://github.com/riscv/riscv-tools

## Contributors
* [Chengyu Song]
* [Hyungon Moon]
* Monjur Alam
* [Insu Yun]
* [Byoungyoung Lee]
* [Taesoo Kim]
* [Wenke Lee]
* [Yunheung Paek]

[Chengyu Song]: <http://www.cs.ucr.edu/~csong/>
[Hyungon Moon]: <https://hyungon-moon.github.io/>
[Insu Yun]: <http://jakkdu.github.io/>
[Byoungyoung Lee]: <https://lifeasageek.github.io/>
[Taesoo Kim]: <https://taesoo.gtisc.gatech.edu>
[Wenke Lee]: <http://wenke.gtisc.gatech.edu>
[Yunheung Paek]: <http://sor.snu.ac.kr/ypaek/>

## Reference
```
@inproceedings{song:hdfi,
  title        = {{HDFI: Hardware-Assisted Data-Fow Isolation}},
  author       = {Chengyu Song and Hyungon Moon and Monjur Alam and Insu Yun and Byoungyoung Lee and Taesoo Kim and Wenke Lee and Yunheung Paek},
  booktitle    = {Proceedings of the 37th IEEE Symposium on Security and Privacy (Oakland)},
  month        = may,
  year         = 2016,
  address      = {San Jose, CA},
}
```
