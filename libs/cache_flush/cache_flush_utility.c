#include <stdlib.h>
#include "cache_flush.h"

const lvl_enum_t cache_lvls[] = {_CF_L1_, _CF_L2_, _CF_L3_, _CF_NONE_};

const char *lvl_to_str(const lvl_enum_t lvl)
{
	switch (lvl)
	{
	case _CF_L1_:
		return "L1";
	case _CF_L2_:
		return "L2";
	case _CF_L3_:
		return "L3";
	case _CF_NONE_:
		return "NONE";
	default:
		return NULL;
	}
}
