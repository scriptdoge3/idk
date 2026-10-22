/*
 * time.c - gmtime()/asctime() for the userland. Linux 0.01 keeps the
 * clock in seconds since 1970 read from the CMOS, which holds local time
 * on most PCs, so there is no time zone handling here.
 */
#include <time.h>
#include <stdio.h>

static int leap(int y)
{
	return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

struct tm * gmtime(const time_t * tp)
{
	static struct tm tm;
	static const char mdays[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
	long t = *tp, days = t / 86400, rem = t % 86400;
	int y = 1970, m, dm;

	if (rem < 0) {
		rem += 86400;
		days--;
	}
	tm.tm_hour = rem / 3600;
	tm.tm_min = rem % 3600 / 60;
	tm.tm_sec = rem % 60;
	tm.tm_wday = (days + 4) % 7;		/* 1970-01-01 was a Thursday */
	if (tm.tm_wday < 0)
		tm.tm_wday += 7;
	while (days >= 365 + leap(y)) {
		days -= 365 + leap(y);
		y++;
	}
	while (days < 0) {
		y--;
		days += 365 + leap(y);
	}
	tm.tm_year = y - 1900;
	tm.tm_yday = days;
	for (m = 0; ; m++) {
		dm = mdays[m] + (m == 1 && leap(y));
		if (days < dm)
			break;
		days -= dm;
	}
	tm.tm_mon = m;
	tm.tm_mday = days + 1;
	tm.tm_isdst = 0;
	return &tm;
}

struct tm * localtime(const time_t * tp)
{
	return gmtime(tp);
}

char * asctime(const struct tm * tm)
{
	static char buf[32];
	static const char * day = "SunMonTueWedThuFriSat";
	static const char * mon = "JanFebMarAprMayJunJulAugSepOctNovDec";

	snprintf(buf, sizeof buf, "%.3s %.3s %2d %02d:%02d:%02d %d\n",
		day + 3 * tm->tm_wday, mon + 3 * tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, 1900 + tm->tm_year);
	return buf;
}

char * ctime(const time_t * tp)
{
	return asctime(gmtime(tp));
}
