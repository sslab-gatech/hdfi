# See LICENSE for license details.

#*****************************************************************************
# fmin.S
#-----------------------------------------------------------------------------
#
# Test f{min|max}.{s|d} instructinos.
#

#include "riscv_test.h"
#include "test_macros.h"

RVTEST_RV64UF
RVTEST_CODE_BEGIN

  #-------------------------------------------------------------
  # Arithmetic tests
  #-------------------------------------------------------------

  TEST_FP_OP2_S( 2,  fmin.s, 0,        1.0,        2.5,        1.0 );
  TEST_FP_OP2_S( 3,  fmin.s, 0,    -1235.1,    -1235.1,        1.1 );
  TEST_FP_OP2_S( 4,  fmin.s, 0,    -1235.1,        1.1,    -1235.1 );
  TEST_FP_OP2_S( 5,  fmin.s, 0,    -1235.1,        NaN,    -1235.1 );
  TEST_FP_OP2_S( 6,  fmin.s, 0, 0.00000001, 3.14159265, 0.00000001 );
  TEST_FP_OP2_S( 7,  fmin.s, 0,       -2.0,       -1.0,       -2.0 );

  TEST_FP_OP2_S(12,  fmax.s, 0,        2.5,        2.5,        1.0 );
  TEST_FP_OP2_S(13,  fmax.s, 0,        1.1,    -1235.1,        1.1 );
  TEST_FP_OP2_S(14,  fmax.s, 0,        1.1,        1.1,    -1235.1 );
  TEST_FP_OP2_S(15,  fmax.s, 0,    -1235.1,        NaN,    -1235.1 );
  TEST_FP_OP2_S(16,  fmax.s, 0, 3.14159265, 3.14159265, 0.00000001 );
  TEST_FP_OP2_S(17,  fmax.s, 0,       -1.0,       -1.0,       -2.0 );

  TEST_FP_OP2_D(22,  fmin.d, 0,        1.0,        2.5,        1.0 );
  TEST_FP_OP2_D(23,  fmin.d, 0,    -1235.1,    -1235.1,        1.1 );
  TEST_FP_OP2_D(24,  fmin.d, 0,    -1235.1,        1.1,    -1235.1 );
  TEST_FP_OP2_D(25,  fmin.d, 0,    -1235.1,        NaN,    -1235.1 );
  TEST_FP_OP2_D(26,  fmin.d, 0, 0.00000001, 3.14159265, 0.00000001 );
  TEST_FP_OP2_D(27,  fmin.d, 0,       -2.0,       -1.0,       -2.0 );

  TEST_FP_OP2_D(32,  fmax.d, 0,        2.5,        2.5,        1.0 );
  TEST_FP_OP2_D(33,  fmax.d, 0,        1.1,    -1235.1,        1.1 );
  TEST_FP_OP2_D(34,  fmax.d, 0,        1.1,        1.1,    -1235.1 );
  TEST_FP_OP2_D(35,  fmax.d, 0,    -1235.1,        NaN,    -1235.1 );
  TEST_FP_OP2_D(36,  fmax.d, 0, 3.14159265, 3.14159265, 0.00000001 );
  TEST_FP_OP2_D(37,  fmax.d, 0,       -1.0,       -1.0,       -2.0 );

  TEST_PASSFAIL

RVTEST_CODE_END

  .data
RVTEST_DATA_BEGIN

  TEST_DATA

RVTEST_DATA_END
