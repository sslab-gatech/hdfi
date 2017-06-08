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

  TEST_ST_OP( 2, ld, sd, 0x00aa00aa00aa00aa, 0,  tdat );
  TEST_ST_OP( 3, ldchk1, sdset1, 0xaa00aa00aa00aa00, 8,  tdat );
  TEST_ST_OP( 4, ldchk0, sdset1, 0x0aa00aa00aa00aa0, 16,  tdat );
  TEST_ST_OP( 5, ld, sdset1, 0xa00aa00aa00aa00a, 24, tdat );

  # Test with negative offset

  TEST_ST_OP( 6, ldchk1, sd, 0x00aa00aa00aa00aa, -24, tdat8 );
  TEST_ST_OP( 7, ldchk0, sd, 0xaa00aa00aa00aa00, -16, tdat8 );
  TEST_ST_OP( 8, ld, sd, 0x0aa00aa00aa00aa0, -8,  tdat8 );
  TEST_ST_OP( 9, ld, sd, 0xa00aa00aa00aa00a, 0,   tdat8 );

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
