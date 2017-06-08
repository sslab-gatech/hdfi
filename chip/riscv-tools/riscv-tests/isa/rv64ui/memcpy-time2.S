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

    j start

setStats:
    li     a7, 1234
    li     a1, 0
    li     a2, 0
    ecall
    ret

start:
    li     t0, 120
    la     t1, tdat
    add    t2, t1, t0
    li     t3, 0x1234567812345678
    
    li     a0, 1
    #jal    setStats

    sdset1 t3, 0(t1)
    sdset1 t3, 8(t1)
    sdset1 t3, 16(t1)
    sdset1 t3, 24(t1)
    sdset1 t3, 32(t1)
    sdset1 t3, 40(t1)
    sdset1 t3, 48(t1)
    sdset1 t3, 56(t1)
    sdset1 t3, 64(t1)
    sdset1 t3, 72(t1)
    sdset1 t3, 80(t1)
    sdset1 t3, 88(t1)
    sdset1 t3, 96(t1)
    sdset1 t3, 104(t1)

    memcpy t2, 0(t1)
    memcpy t2, 8(t1)
    memcpy t2, 16(t1)
    memcpy t2, 24(t1)
    memcpy t2, 32(t1)
    memcpy t2, 40(t1)
    memcpy t2, 48(t1)
    memcpy t2, 56(t1)
    memcpy t2, 64(t1)
    memcpy t2, 72(t1)
    memcpy t2, 80(t1)
    memcpy t2, 88(t1)
    memcpy t2, 96(t1)
    memcpy t2, 104(t1)

    li     a0, 0
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
