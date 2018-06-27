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
_unused_ int file_regex(const char *file, const char *regex);
#endif /* __linux__ */

/*
 * test(1) clone like file test helper
 * @mode char like test (whithout the dash) 'e', 'd', etc;
 * @return bool;
 */
_unused_ int file_test(const char *pathname, int mode);

#ifndef HAVE_GETLINE
_unused_ ssize_t getline(char **buf, size_t *len, FILE *stream);
#endif

/*
 * (WARN: only first to second character are used!)
 * simple yes/no helper
 * @return true/false;
 */
_unused_ int sv_yesno(const char *str);

/*
 * tiny helper to get terminal cols
 * @retrun terminal cols
 */
_unused_ int get_term_cols(void);

/*
 * get shell string value, removing trailing white spaces and quote
 * @str string;
 * @return string value
 */
_unused_ char *shell_string_value(char *str);

#ifdef __cplusplus
}
#endif

#endif /* HELPER_H */
