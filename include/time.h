#ifndef _TIME_H
#define _TIME_H

#ifndef _TIME_T
#define _TIME_T
typedef long time_t;
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

#define CLOCKS_PER_SEC 100

typedef long clock_t;

struct tm {
	time_t tm_sec;
	time_t tm_min;
	time_t tm_hour;
	time_t tm_mday;
	time_t tm_mon;
	time_t tm_year;
	time_t tm_wday;
	time_t tm_yday;
	time_t tm_isdst;
};

clock_t clock(void);
extern time_t time(time_t* tp);
double difftime(time_t time2, time_t time1);
time_t mktime(struct tm * tp);

char* asctime(const struct tm * tp);
char* ctime(const time_t * tp);
struct tm* gmtime(const time_t *tp);
struct tm* localtime(const time_t * tp);
size_t strftime(char * s, size_t smax, const char * fmt, const struct tm * tp);
void tzset(void);

#endif
