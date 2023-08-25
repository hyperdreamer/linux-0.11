#pragma once

#include <sys/types.h>	/* I know - shouldn't do this, but .. */

struct utimbuf {
	time_t actime;
	time_t modtime;
};

extern int utime(const char* filename, struct utimbuf* times);

