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
  TEST_CASE( 1, x6, 0x1234567812345678, \
    la  x1, tdat1; \
    li  x2, 0x1234567812345678; \
    addi x3, x1, 32; \
    sdset1 x2, 0(x1); \
    ldchk1 x4, 0(x1); \
    sdset1 x4, 0(x3); \
    li  x4, 0X8765432187654321; \
    sdset1 x4, 0(x1) ;\
    ldchk1 x5, 0(x1) ;\
    ldchk1 x6, 0(x3); \
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
