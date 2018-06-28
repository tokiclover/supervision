/*
 * Copyright (C) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)heleper.h  0.13.0 2016/12/28
 */

#ifndef HELPER_H
#define HELPER_H

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#ifdef __linux__
# include <regex.h>
#endif
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux__
/* look for @regex regular exppression in @file
 * @return: 0 when found; 1 when not; <= -1/-errno on failure;
 */
__attribute__((__unused__)) int file_regex(const char *file, const char *regex);
#endif /* __linux__ */

/*
 * test(1) clone like file test helper
 * @mode char like test (whithout the dash) 'e', 'd', etc;
 * @return bool;
 */
__attribute__((__unused__)) int file_test(const char *pathname, int mode);

#ifndef HAVE_GETLINE
__attribute__((__unused__)) ssize_t getline(char **buf, size_t *len, FILE *stream);
#endif

/*
 * (WARN: only first to second character are used!)
 * simple yes/no helper
 * @return true/false;
 */
__attribute__((__unused__)) int sv_yesno(const char *str);

/*
 * tiny helper to get terminal cols
 * @retrun terminal cols
 */
__attribute__((__unused__)) int get_term_cols(void);

/*
 * get shell string value, removing trailing white spaces and quote
 * @str string;
 * @return string value
 */
__attribute__((__unused__)) char *shell_string_value(char *str);

#ifdef __cplusplus
}
#endif

#endif /* HELPER_H */
