// -*- tab-width: 2; indent-tabs-mode: nil -*-
// vi: set et ts=2 sw=2 sts=2:
#ifndef READCONFIG_H
#define READCONFIG_H

#ifndef HAVE_STRCHRNUL
#define strchrnul gitstrchrnul
static inline char *gitstrchrnul(const char *s, int c)
{
	while (*s && *s != c)
		s++;
	return (char *)s;
}
#endif

/*
 * Like skip_prefix, but promises never to read past "len" bytes of the input
 * buffer, and returns the remaining number of bytes in "out" via "outlen".
 */
static inline int skip_prefix_mem(const char *buf, size_t len,
				  const char *prefix,
				  const char **out, size_t *outlen)
{
	size_t prefix_len = strlen(prefix);
	if (prefix_len <= len && !memcmp(buf, prefix, prefix_len)) {
		*out = buf + prefix_len;
		*outlen = len - prefix_len;
		return 1;
	}
	return 0;
}

enum {
  COLOR_MAXLEN = 75,
  COLOR_FOREGROUND_ANSI = 30,
	COLOR_FOREGROUND_BRIGHT_ANSI = 90,
};

/* An individual foreground or background color. */
struct color {
	enum {
		COLOR_UNSPECIFIED = 0,
		COLOR_NORMAL,
		COLOR_ANSI, /* basic 0-7 ANSI colors */
		COLOR_256,
		COLOR_RGB
	} type;
	/* The numeric value for ANSI and 256-color modes */
	unsigned char value;
	/* 24-bit RGB color values */
	unsigned char red, green, blue;
};

int color_parse_mem(const char* value, int value_len, char* dst);

void* expand_user_path(const char* path, char* dest);

int read_config(void);

#endif
