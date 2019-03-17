/*
 * Copyright (c) 2016-2019 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * new BSD License included in the distriution of this package.
 *
 * @(#)timespec.h  0.15.0 2019/03/14
 */

#ifndef TIMESPEC_H
#define TIMESPEC_H

#include <limits.h>
#include <sys/types.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TIMESPEC_NSEC 8LU 
#define TIMESPEC_SIZE 16LU
#define TIMESPEC_NSEC_MAX 999999999LU
#define TIMESPEC_APPROX(ta) ((ta)->ta_sec+(ta)->ta_nsec > 50000000LU ? 1LU : OLU)

#define TIMESPEC(ta) clock_gettime(CLOCK_REALTIME, ta)

extern uint64_t timespec_pack_sec  (char *restrict s, register uint64_t u);
extern uint64_t timespec_unpack_sec(char *restrict s) __attribute__((__unused__));

extern void timespec_add(struct timespec *restrict ts, struct timespec *restrict u, struct timespec *restrict v);
extern void timespec_sub(struct timespec *restrict ts, struct timespec *restrict u, struct timespec *restrict v);
extern int  timespec_cmp(struct timespec *restrict ts, struct timespec *restrict u) __attribute__((__unused__));

extern void timespec_pack  (char *restrict s, struct timespec *restrict ts);
extern void timespec_unpack(char *restrict s, struct timespec *restrict ts);

#ifdef __cplusplus
}
#endif
#endif /*TIMESPEC_H*/
