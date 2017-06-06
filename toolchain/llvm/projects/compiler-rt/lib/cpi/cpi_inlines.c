#include "cpi.h"

#include "common_inlines.c"

#if defined(CPI_LOOKUP_TABLE)
# include "lookuptable_inlines.c"
#elif defined(CPI_SIMPLE_TABLE)
# include "simpletable_inlines.c"
#else
# include "hashtable_inlines.c"
#endif
