// -*- tab-width: 2; indent-tabs-mode: nil -*-
// vi: set et ts=2 sw=2 sts=2:

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <pwd.h>

#include "util.h"
#include "options.h"
#include "readconfig.h"
// include from submodule inih
#include "../inih/ini.h"

/*
 * "word" is a buffer of length "len"; does it match the NUL-terminated
 * "match" exactly?
 */
static int match_word(const char *word, int len, const char *match)
{
	return !strncasecmp(word, match, len) && !match[len];
}

/*
 * If an ANSI color is recognized in "name", fill "out" and return 0.
 * Otherwise, leave out unchanged and return -1.
 */
static int parse_ansi_color(struct color *out, const char *name, int len)
{
	/* Positions in array must match ANSI color codes */
	static const char * const color_names[] = {
		"black", "red", "green", "yellow",
		"blue", "magenta", "cyan", "white"
	};
	int i;
	int color_offset = COLOR_FOREGROUND_ANSI;

	if (strncasecmp(name, "bright", 6) == 0) {
		color_offset = COLOR_FOREGROUND_BRIGHT_ANSI;
		name += 6;
		len -= 6;
	}
	// for (i = 0; i < ARRAY_SIZE(color_names); i++) {
	for (i = 0; i < 8; i++) {
		if (match_word(name, len, color_names[i])) {
			out->type = COLOR_ANSI;
			out->value = i + color_offset;
			return 0;
		}
	}
	return -1;
}

static int parse_color(struct color *out, const char *name, int len)
{
	char *end;
	long val;

#if 0
	/* First try the special word "normal"... */
	if (match_word(name, len, "normal")) {
		out->type = COLOR_NORMAL;
		return 0;
	}

	/* Try a 24-bit RGB value */
	if (len == 7 && name[0] == '#') {
		if (!get_hex_color(name + 1, &out->red) &&
		    !get_hex_color(name + 3, &out->green) &&
		    !get_hex_color(name + 5, &out->blue)) {
			out->type = COLOR_RGB;
			return 0;
		}
	}
#endif

	/* Then pick from our human-readable color names... */
	if (parse_ansi_color(out, name, len) == 0) {
		return 0;
	}

	/* And finally try a literal 256-color-mode number */
	val = strtol(name, &end, 10);
	if (end - name == len) {
		/*
		 * Allow "-1" as an alias for "normal", but other negative
		 * numbers are bogus.
		 */
		if (val < -1)
			; /* fall through to error */
		else if (val < 0) {
			out->type = COLOR_NORMAL;
			return 0;
		/* Rewrite 0-7 as more-portable standard colors. */
		} else if (val < 8) {
			out->type = COLOR_ANSI;
			out->value = val + COLOR_FOREGROUND_ANSI;
			return 0;
		/* Rewrite 8-15 as more-portable aixterm colors. */
		} else if (val < 16) {
			out->type = COLOR_ANSI;
			out->value = val - 8 + COLOR_FOREGROUND_BRIGHT_ANSI;
			return 0;
		} else if (val < 256) {
			out->type = COLOR_256;
			out->value = val;
			return 0;
		}
	}

	return -1;
}

static int parse_attr(const char *name, size_t len)
{
	static const struct {
		const char *name;
		size_t len;
		int val, neg;
	} attrs[] = {
#define ATTR(x, val, neg) { (x), sizeof(x)-1, (val), (neg) }
		ATTR("bold",      1, 22),
		ATTR("dim",       2, 22),
		ATTR("italic",    3, 23),
		ATTR("ul",        4, 24),
		ATTR("blink",     5, 25),
		ATTR("reverse",   7, 27),
		ATTR("strike",    9, 29)
#undef ATTR
	};
	int negate = 0;
	int i;

	if (skip_prefix_mem(name, len, "no", &name, &len)) {
		skip_prefix_mem(name, len, "-", &name, &len);
		negate = 1;
	}

	// for (i = 0; i < ARRAY_SIZE(attrs); i++) {
	for (i = 0; i < 7; i++) {
		if (attrs[i].len == len && !memcmp(attrs[i].name, name, len))
			return negate ? attrs[i].neg : attrs[i].val;
	}
	return -1;
}

static int color_empty(const struct color *c)
{
	return c->type <= COLOR_NORMAL;
}

int color_parse_mem(const char* value, int value_len, char* dest)
{
	const char *ptr = value;
	int len = value_len;
	unsigned int attr = 0;
	struct color fg = { COLOR_UNSPECIFIED };
	struct color bg = { COLOR_UNSPECIFIED };

	while (len > 0 && isspace(*ptr)) {
		ptr++;
		len--;
	}

	/* [fg [bg]] [attr]... */
	while (len > 0) {
		const char *word = ptr;
		struct color c = { COLOR_UNSPECIFIED };
		int val, wordlen = 0;

		while (len > 0 && !isspace(word[wordlen])) {
			wordlen++;
			len--;
		}

		ptr = word + wordlen;
		while (len > 0 && isspace(*ptr)) {
			ptr++;
			len--;
		}

		if (!parse_color(&c, word, wordlen)) {
			if (fg.type == COLOR_UNSPECIFIED) {
				fg = c;
				continue;
			}
			if (bg.type == COLOR_UNSPECIFIED) {
				bg = c;
				continue;
			}
			/* goto bad; */
		}
		val = parse_attr(word, wordlen);
		if (0 <= val)
			attr |= (1 << val);
		/* else */
		/* 	goto bad; */
	}

  char sequence_buffer[32];

  if(attr || !color_empty(&fg) || !color_empty(&bg)) {
		int sep = 0;
		int i;

    strcpy(dest, "\033[");

		for (i = 0; attr; i++) {
			unsigned bit = (1 << i);
			if (!(attr & bit))
				continue;
			attr &= ~bit;
			if (sep++)
        strcat(dest, ";");
      sprintf(sequence_buffer, "%d", i);
      strcat(dest, sequence_buffer);
		}
		if (!color_empty(&fg)) {
			if (sep++)
        strcat(dest, ";");
      sprintf(sequence_buffer, "%d", fg.value);
      strcat(dest, sequence_buffer);
    }
		if (!color_empty(&bg)) {
      if (sep++)
        strcat(dest, ";");
      sprintf(sequence_buffer, "%d", bg.value);
      strcat(dest, sequence_buffer);
    }
    strcat(dest, "m");
  }

  return 0;
}

void* expand_user_path(const char* path, char* dest)
{
  const char* to_copy = path;
  if(path == NULL)
    return dest;
  if(path[0] == '~')
  {
    const char* first_slash = strchrnul(path, '/');
    const char* username = path + 1;
    size_t username_len = first_slash - username;
    // the ~/ case
    if(username_len == 0)
    {
      const char* home = getenv("HOME");
      if(!home)
        return dest;
      strcpy(dest,home);
    }
    // the ~user case
    else
    {
      struct passwd* pw;
      char* username_z = memcpy(malloc(username_len), username, username_len);
      pw = getpwnam(username_z);
      free(username_z);
      if(!pw)
        return dest;
      strcpy(dest, pw->pw_dir);
    }
    to_copy = first_slash;
  }
  strcat(dest,to_copy);
  return dest;
}

static int handler(void* user, const char* section, const char* name,
                   const char* value)
{
  cli_options* pconfig = (cli_options*)user;
#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
  if(MATCH("color", "linenumber"))
  {
    char ansi_color[COLOR_MAXLEN];
    color_parse_mem(value, strlen(value), ansi_color);
    free(pconfig->color_line_number);
    ag_asprintf(&pconfig->color_line_number, "%s", ansi_color);
  }
  else if(MATCH("color", "match"))
  {
    char ansi_color[COLOR_MAXLEN];
    color_parse_mem(value, strlen(value), ansi_color);
    free(pconfig->color_match);
    ag_asprintf(&pconfig->color_match, "%s", ansi_color);
  }
  else if(MATCH("color", "filename"))
  {
    char ansi_color[COLOR_MAXLEN];
    color_parse_mem(value, strlen(value), ansi_color);
    free(pconfig->color_path);
    ag_asprintf(&pconfig->color_path, "%s", ansi_color);
  }
  else if(MATCH("search", "case"))
  {
    if(strcmp(value,"sensitive") == 0)
      pconfig->casing = CASE_SENSITIVE;
    if(strcmp(value,"insensitive") == 0)
      pconfig->casing = CASE_INSENSITIVE;
  }
  else if(MATCH("output", "pager"))
  {
    free(pconfig->pager);
    pconfig->pager = ag_strdup(value);
  }
  else
  {
    return 0; // unknown section/name error
  }
  return 1;
}

int read_config()
{
  const char* path = "~/.agrc";
  char full_config_path[256];
  expand_user_path(path, full_config_path);

  if(ini_parse(full_config_path, handler, &opts) < 0)
  {
    log_err("Cannot load '%s'!\n", full_config_path);
    return 0;
  }
  return 1;
}
