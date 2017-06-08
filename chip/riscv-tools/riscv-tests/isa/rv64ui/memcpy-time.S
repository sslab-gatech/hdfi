# See LICENSE for license details.

#*****************************************************************************
# memcpy-time.S
#-----------------------------------------------------------------------------
#
# Test tagged memory instruction.
#

#include "riscv_test.h"
#include "test_macros.h"

RVTEST_RV64U
RVTEST_CODE_BEGIN

    j      start

setStats:
    li     a7, 1234
    li     a1, 0
    li     a2, 0
    ecall
    ret

start:
    li     t0, 32
    la     t1, tdat
    add    t2, t1, t0
    addi   t3, zero, 0

    #li     a0, 1
    #jal    setStats
      
loop:
    memcpy t2, 0(t1)
    addi   t1, t1, 8
    addi   t2, t2, 8
    addi   t3, t3, 8
    bltu   t3, t0, loop

    #li     a0, 0
    #jal    setStats

RVTEST_PASS
RVTEST_CODE_END

  .data
RVTEST_DATA_BEGIN

  TEST_DATA

tdat:
tdat1:  .dword 0xdeadbeefdeadbeef
tdat2:  .dword 0xdeadbeefdeadbeef
tdat3:  .dword 0xdeadbeefdeadbeef
tdat4:  .dword 0xdeadbeefdeadbeef
tdat5:  .dword 0xdeadbeefdeadbeef
tdat6:  .dword 0xdeadbeefdeadbeef
tdat7:  .dword 0xdeadbeefdeadbeef
tdat8:  .dword 0xdeadbeefdeadbeef
tdat9:  .dword 0xdeadbeefdeadbeef
tdat10: .dword 0xdeadbeefdeadbeef

RVTEST_DATA_END
