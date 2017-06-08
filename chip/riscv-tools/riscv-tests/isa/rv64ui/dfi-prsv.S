# See LICENSE for license details.

#*****************************************************************************
# sd.S
#-----------------------------------------------------------------------------
#
# Test tagged memory instruction.
#

#include "riscv_test.h"
#include "test_macros.h"

RVTEST_RV64U
RVTEST_CODE_BEGIN

  #-------------------------------------------------------------
  # Basic tests
  #-------------------------------------------------------------
  TEST_CASE( 1, x2, 0x1234567812345679, \
    la  x1, tdat9; \
    li  x2, 0x1234567812345678; \
    addi x3, x1, -32; \
    sdset1 x2, 0(x1); \
    ldchk1 x2, 0(x1); \
    sdset1 x2, 0(x1); \
    memcpy x3, 0(x1); \
    ldchk1 x2, 0(x3); \
    addi x2, x2, 1; \
  )

  TEST_CASE( 2, x1, 0x8765432187654322, \
    la  x1, tdat9; \
    li  x2, 0x8765432187654321; \
    addi x3, x1, -32; \
    addi x4, x1, 16; \
    sdset1 x2, 0(x1); \
    memcpy x3, 0(x1); \
    memcpy x4, 0(x3); \
    ldchk1 x1, 0(x4); \
    addi x1, x1, 1; \
  )

  TEST_CASE( 3, x4, 0x1111222233334444, \
    la  x1, tdat1; \
    la  x2, tdat6; \
    li  x3, 0x1111222233334444; \
    sdset1 x3, 16(x1); \
    memcpy x2, 16(x1); \
    ldchk1 x4, 16(x2); \
  )

  TEST_PASSFAIL

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
