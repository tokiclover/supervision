/*
 * Copyright (C) 2016 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision (Scripts Framework).
 *
 * The supervision framework is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 */

#include "error.h"
#include "helper.h"

char *shell_string_value(char *str)
{
	if (!str)
		return NULL;
	char *ptr = str, *end = str+strlen(str)-1;

	if (*ptr == '\'' || *ptr == '\"')
		ptr++;
	for ( ; *ptr == ' '; *ptr++)
		;
	if (*end == '\'' || *end == '\"')
		*end-- = '\0';
	for ( ; *end == ' '; *end-- = '\0')
		;
	if (*ptr != '\0')
		return ptr;
	return NULL;
}

__UNUSED__ int file_test(const char *pathname, int mode)
{
	static struct stat st_buf;
	static char *path;
	static int retval;
	size_t len;

	if (path == NULL) {
		len = strlen(pathname);
		path = err_malloc(len+1);
		memcpy(path, pathname, len+1);
	}

	if (strcmp(path, pathname) != 0) {
		memset(&st_buf, 0, sizeof(st_buf));
		len = strlen(pathname);
		path = err_realloc(path, len+1);
		memcpy(path, pathname, len+1);
		retval = (mode == 'h' || mode == 'L') ? lstat(pathname, &st_buf) : \
				 stat(pathname, &st_buf);
	}

	if (retval < 0) {
		errno = EBADF;
		return 0;
	}
	switch (mode) {
		case  0 :
		case 'e': return 1;
		case 'f': return S_ISREG(st_buf.st_mode);
		case 'd': return S_ISDIR(st_buf.st_mode);
		case 'b': return S_ISBLK(st_buf.st_mode);
		case 'c': return S_ISCHR(st_buf.st_mode);
		case 'p': return S_ISFIFO(st_buf.st_mode);
		case 's': return st_buf.st_size > 0;
		case 'h':
		case 'L': return S_ISLNK(st_buf.st_mode);
		case 'S': return S_ISSOCK(st_buf.st_mode);
		case 'r': return st_buf.st_mode & S_IRUSR;
		case 'w': return st_buf.st_mode & S_IWUSR;
		case 'x': return st_buf.st_mode & S_IXUSR;
		case 'g': return st_buf.st_mode & S_ISGID;
		case 'u': return st_buf.st_mode & S_ISUID;
		default: errno = EINVAL; return 0;
	}
}

int get_term_cols(void)
{
	struct winsize winsz;
	char *ptr = getenv("COLUMNS");
	int col;

	if (ptr && (col = atoi(ptr)))
		return col;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsz) == 0)
		return winsz.ws_col;
	/* default term cols */
	return 80;
}

ssize_t rs_getline(FILE *stream, char **buf, size_t *size)
{
	size_t len = 0;
	if (!stream) {
		errno = EBADF;
		return -1;
	}

	*buf = err_realloc(*buf, BUFSIZ);
	memset(*buf, 0, BUFSIZ);

	while (fgets(*buf+len, BUFSIZ, stream)) {
		if (*(*buf) == '\n')
			continue;

		len += strlen(*buf);
		if (*(*buf+len-1) == '\n') {
			*(*buf+len-1) = '\0';
			--len;
			goto retline;
		}
		else if (feof(stream)) {
			goto retline;
		}
		else {
			*buf = err_realloc(*buf, len+1+BUFSIZ);
			memset(*buf+len+1, 0, BUFSIZ);
		}
	}
	len = 0;
	goto retline;

retline:
	*buf = err_realloc(*buf, len);
	*size = len;
	return len;
}

int rs_yesno(const char *str)
{
	if (!str) {
		errno = ENOENT;
		return 0;
	}

	switch(str[0]) {
		case 'y':
		case 'Y':
		case 't':
		case 'T':
		case 'e':
		case 'E':
		case '1':
			return 1;
		case 'n':
		case 'N':
		case 'f':
		case 'F':
		case 'd':
		case 'D':
		case '0':
			return 0;
		case 'o':
		case 'O':
			switch(str[1]) {
				case 'n':
				case 'N':
					return 1;
				case 'f':
				case 'F':
					return 0;
				default:
					errno = EINVAL;
					return 0;
			}
		default:
			errno = EINVAL;
			return 0;
	}
}

