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
	while (*ptr == ' ')
		(void)*ptr++;
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
	static int retval, setup = 1;
	static int R = S_IRUSR | S_IRGRP | S_IROTH,
			   W = S_IWUSR | S_IWGRP | S_IWOTH,
			   X = S_IXUSR | S_IXGRP | S_IXOTH;
	size_t len;

	if (access(pathname, F_OK))
		return 0;
	if (path == NULL) {
		len = strlen(pathname);
		path = err_malloc(len+1);
		memcpy(path, pathname, len+1);
	}

	if (setup || strcmp(path, pathname) != 0) {
		memset(&st_buf, 0, sizeof(st_buf));
		len = strlen(pathname);
		path = err_realloc(path, len+1);
		memcpy(path, pathname, len+1);

		retval = (mode == 'h' || mode == 'L') ? lstat(pathname, &st_buf) : \
				 stat(pathname, &st_buf);
		if (retval < 0)
			return 0;
		setup = 0;
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
		case 'r': return st_buf.st_mode & R;
		case 'w': return st_buf.st_mode & W;
		case 'x': return st_buf.st_mode & X;
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
	char *ptr;
	if (!stream) {
		errno = EBADF;
		return -1;
	}
	*size = 0;

	*buf = err_realloc(*buf, BUFSIZ);
	memset(*buf, 0, BUFSIZ);

	while (fgets(*buf+*size, BUFSIZ, stream)) {
		if (**buf == '\n')
			continue;

		*size += strlen(*buf);
		ptr = *buf+*size-1;
		if (*ptr == '\n') {
			*ptr = '\0';
			goto retline;
		}
		else if (feof(stream)) {
			goto retline;
		}
		else {
			*buf = err_realloc(*buf, *size+1+BUFSIZ);
			memset(*buf+*size+1, 0, BUFSIZ);
		}
	}
	goto retline;

retline:
	*buf = err_realloc(*buf, *size);
	return *size;
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

