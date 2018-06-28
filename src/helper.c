/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)helper.c  0.13.0 2016/12/28
 */

#include "error.h"
#include "helper.h"

__attribute__((__unused__)) char *shell_string_value(char *str)
{
	char *ptr = str, *end;
	if (!str)
		return NULL;
	if      ((end = strrchr(str, '"')))  ;
	else if ((end = strrchr(str, '\''))) ;
	else if ((end = strrchr(str, '#')))  ;
	else      end = str+strlen(str)-1    ;

	switch (*ptr) {
	case '\'':
	case '"' :
	case '\t':
	case ' ' :
		ptr++;
		break;
	}
	while (*ptr == ' ' || *ptr == '\t')
		ptr++;
	switch (*end) {
	case '\'':
	case '"' :
	case '\t':
	case '\n':
	case ' ' :
		*end-- = '\0';
		break;
	}
	while (*end == ' ' || *end == '\t')
		*end-- = '\0';
	if (*ptr != '\0')
		return ptr;
	return NULL;
}

#ifdef __linux__
__attribute__((__unused__)) int file_regex(const char *file, const char *regex)
{
	FILE *fp;
	char *end, *line = NULL, *ptr;
	size_t len = 0;
	int retval = 0;
	regex_t re;

	if (!(fp = fopen(file, "r")))
		return -EBADF;

	if ((retval = regcomp(&re, regex, REG_EXTENDED | REG_NOSUB))) {
		line = err_malloc(256);
		regerror(retval, (const regex_t *)&re, line, 256);
		ERR("%s: Failed to compile `%s': %s\n", __func__, regex, line);
		free(line);
		return -EINVAL;
	}

	while (getline(&line, &len, fp) > 0) {
		ptr = line;
		end = line+len;
		/* handle null terminated strings */
		do {
			if (!regexec(&re, ptr, 0, NULL, 0))
				goto found;
			ptr += strlen(ptr)+1;
			/* find next string */
			while (*ptr == '\0' && ptr++ < end)
				;
		} while (ptr < end);
	}
	if (len) free(line);
	retval = 1;
found:
	fclose(fp);
	regfree(&re);
	return retval;
}
#endif /* __linux__ */

__attribute__((__unused__)) int file_test(const char *pathname, int mode)
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

	if (setup || strcmp(path, pathname)) {
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
		default: return 0;
	}
}

__attribute__((__unused__)) int get_term_cols(void)
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

#ifndef HAVE_GETLINE
__attribute__((__unused__)) ssize_t getline(char **buf, size_t *len, FILE *stream)
{
	int c;
	char *p;
	size_t s = *len;

	if (!s) s += BUFSIZ;
	if (!*buf || !*len)
		*buf = err_realloc(*buf, s);
	p = *buf;
	*len = 0;

	while (!feof(stream) && (c = getc(stream)) != EOF) {
		if (c == '\n') {
			if (!*len) /* skip empty line */
				continue;
			break;
		}
		*p = (unsigned char)c;
		p++; ++*len;
		if (*len >= s) {
			s += BUFSIZ;
			*buf = err_realloc(*buf, s);
			p = *buf+*len;
		}
	}

	if (*len) *p = '\0';
	*buf = err_realloc(*buf, *len);
	return *len;
}
#endif

__attribute__((__unused__)) int sv_yesno(const char *str)
{
	if (!str)
		return 0;

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
			}
		default:
			return 0;
	}
}

