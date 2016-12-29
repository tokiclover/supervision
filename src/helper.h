/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)heleper.h  0.13.0 2016/12/28
 */

#ifndef HELPER_H
#define HELPER_H

#include "error.h"
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/termios.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * test(1) clone like file test helper
 * @mode char like test (whithout the dash) 'e', 'd', etc;
 * @return bool;
 */
_unused_ int file_test(const char *pathname, int mode);

ssize_t sv_getline(FILE *stream, char **buf, size_t *size);

/*
 * (WARN: only first to second character are used!)
 * simple yes/no helper
 * @return true/false;
 */
int sv_yesno(const char *str);

/*
 * tiny helper to get terminal cols
 * @retrun terminal cols
 */
int get_term_cols(void);

/*
 * get shell string value, removing trailing white spaces and quote
 * @str string;
 * @return string value
 */
char *shell_string_value(char *str);

#ifdef __cplusplus
}
#endif

#endif /* HELPER_H */
