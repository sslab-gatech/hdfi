#include "cpi.h"

#include "common_inlines.c"
#include "common.c"

#if defined(CPI_LOOKUP_TABLE)
# include "lookuptable_inlines.c"
# include "lookuptable.c"
#elif defined(CPI_SIMPLE_TABLE)
# include "simpletable_inlines.c"
# include "simpletable.c"
#else
# include "hashtable_inlines.c"
# include "hashtable.c"
#endif
