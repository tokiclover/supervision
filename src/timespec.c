/*
 * Copyright (c) 2016-2019 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * new BSD License included in the distriution of this package.
 *
 * @(#)timespec.c  0.15.0 2019/03/14
 */


#include "timespec.h"

uint64_t timespec_pack_sec(char *restrict s, register uint64_t u)
{
	register int o = TIMESPEC_NSEC;
	while (o--) s[o] = u & 255, u >>= CHAR_BIT;
	s[o] = u;
	return u;
}

__attribute__((__unused__)) uint64_t timespec_unpack_sec(char *restrict s)
{
	register uint64_t u;
	register int o = 0;
	u = (unsigned char)s[o];
	while (o++ < TIMESPEC_NSEC) u <<= CHAR_BIT, u += (unsigned char)s[o];
	return u;
}

__attribute__((__unused__)) int timespec_cmp(struct timespec *restrict u, struct timespec *restrict v)
{
	if (u->tv_sec  < v->tv_sec ) return -1;
	if (u->tv_sec  > v->tv_sec ) return  1;
	if (u->tv_nsec < v->tv_nsec) return -1;
	if (u->tv_nsec > v->tv_nsec) return  1;
	return 0;
}

void timespec_pack(char *restrict s, struct timespec *restrict ts)
{
	register char *p = s;
	timespec_pack_sec(p, (uint64_t)ts->tv_sec  & UINT64_MAX);
	p += TIMESPEC_NSEC;
	timespec_pack_sec(p, (uint64_t)ts->tv_nsec & UINT64_MAX);
	p += TIMESPEC_NSEC;
}

void timespec_add(struct timespec *restrict ts, struct timespec *restrict u, struct timespec *restrict v)
{
	ts->tv_sec = u->tv_sec + v->tv_sec;

	if (TIMESPEC_NSEC_MAX <= u->tv_nsec) {
		ts->tv_sec++;
		ts->tv_nsec = v->tv_nsec;
	}
	else if (TIMESPEC_NSEC_MAX <= v->tv_nsec) {
		ts->tv_sec++;
		ts->tv_nsec = u->tv_nsec;
	}
	else {
		if ((u->tv_nsec + v->tv_nsec) > TIMESPEC_NSEC_MAX) {
			ts->tv_sec++;
			ts->tv_nsec = u->tv_nsec > v->tv_nsec ?
				u->tv_nsec - TIMESPEC_NSEC_MAX - 1LU + v->tv_nsec :
				v->tv_nsec - TIMESPEC_NSEC_MAX - 1LU + u->tv_nsec;
		}
		else
			ts->tv_nsec = u->tv_nsec + v->tv_nsec;
	}
}

void timespec_sub(struct timespec *restrict ts, struct timespec *restrict u, struct timespec *restrict v)
{
	ts->tv_sec = u->tv_sec - v->tv_sec;

	if (TIMESPEC_NSEC_MAX <= u->tv_nsec) {
		ts->tv_nsec = u->tv_nsec - v->tv_nsec;
	}
	else if (TIMESPEC_NSEC_MAX <= v->tv_nsec) {
		if (ts->tv_sec) ts->tv_sec--;
		ts->tv_nsec = u->tv_nsec;
	}
	else {
		if (u->tv_nsec > v->tv_nsec)
			ts->tv_nsec = u->tv_nsec - v->tv_nsec;
		else {
			if (ts->tv_sec) ts->tv_sec--;
			ts->tv_nsec = TIMESPEC_NSEC_MAX + 1LU + (u->tv_nsec - v->tv_nsec);
		}
	}
}

