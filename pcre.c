/*
 *  GT.M PCRE Extension
 *  Copyright (C) 2012 Piotr Koper <piotr.koper@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


/*
 * GT.M PCRE Extension
 *
 * See code comments, pcre.m, pcreexamples.m for the details on API, examples,
 * design decisions, quirks and tips.
 *
 */


#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <pcre.h>
#include <locale.h>
#include <gtmxc_types.h>


/*
 * GCC specific
 */
#define UNUSED __attribute__ ((unused))


/*
 * Error codes
 */

/*
 * M -> C:
 * :s/^U\([^ ]\+\)[[:space:]]*;"%PCRE-E-\([^,]*\), \(.*\)/#define E_\2 -\1 \/\* \3 \*\//
 */
#define E_ARGSMALL	-16384	/* Actual argument count is too small */
#define E_OPTNAME	-16385	/* Unknown option name ... */
#define E_OBJLIMIT	-16386	/* Maximum number of objects exceeded */
#define E_INVREF	-16387	/* Invalid object reference */
#define E_INTBUF	-16388	/* Internal buffer too small */
#define E_MALLOC	-16389	/* Could not allocate memory */
#define E_STUDY		-16390	/* Pattern study failed: ... */
#define E_LOCALE	-16391	/* Invalid locale name */
#define E_COMPILE	-16392	/* Pattern compilation failed: ... */
#define E_LENGTH	-16393	/* Invalid length value specified */

#define E_BASE	-16400


/*
 * Validation
 */

#define check_argc(x) do { \
		if (argc != x) \
			return E_ARGSMALL; \
	} while (0)

#define check_param(x) do { \
		if ((x) != 0) \
			return E_OPTNAME; \
	} while (0)

#define check_malloc(x) do { \
		if ((x) == NULL) \
			return E_MALLOC; \
	} while (0)


/*
 * String functions
 */
int
strncopy (char *p, const char *s, ssize_t n)
{
	while (n > 1)
	{
		if ((*p = *s) == '\0')
			return 0;
		p++;
		s++;
		n--;
	}
	*p = '\0';
	return E_INTBUF;
}

char *
strpncopy (char *p, char *s, ssize_t *n)
{
	while (*n > 1)
	{
		if ((*p = *s) == '\0')
			return p;
		p++;
		s++;
		(*n)--;
	}
	*p = '\0';
	return NULL;
}


/*
 * Named options
 */
#define list_delimiter "|"

typedef struct
{
	char *name;

	/*
	 * NOTE: options parameter for pcre_compile(), pcre_exec() and other
	 * functions is of type int, but many option flags are 32 bit long,
	 * for compatibility with PCRE int is used as a storage for options.
	 */
	int value;
}
param;

#define get_param(x) _get_param (x##_param, sizeof (x##_param), x##_name, &x)

static int
_get_param (param *p, size_t s, char *name, int *value)
{
	unsigned int i;
	for (i = 0; i < s / sizeof (param); i++)
		if (strcasecmp (p[i].name, name) == 0)
		{
			*value = p[i].value;
			return 0;
		}
	return E_OPTNAME;
}

#define get_flags(x) _get_flags (x##_param, sizeof (x##_param), x##_name, invalid_option, &x)
static int
_get_flags (param *param, size_t s, char *name, char *invalid, int *value)
{
	int flags = 0;
	int f;
	char *p = strtok (name, list_delimiter);
	while (p != NULL)
	{
		if (_get_param (param, s, p, &f) != 0)
		{
			strncopy (invalid, p, 32);
			return E_OPTNAME;
		}
		flags |= f;
		p = strtok (NULL, list_delimiter);
	}
	*value = flags;
	return 0;
}


/*
 * Reference list functions
 */
#define push_list(x, list, count) list[count++] = x

#define check_list(count, limit) \
	do { \
		if (count >= limit) \
			return E_OBJLIMIT; \
	} while (0)

static int
in_list (void *p, void **list, int *count, int remove)
{
	int i;
	for (i = 0; i < *count; i++)
		if (list[i] == p)
		{
			if (remove)
				list[i] = list[--(*count)];
			return 1;
		}
	return 0;
}


/*
 * Object type
 */
typedef struct
{
	pcre *re;
	pcre_extra *extra;
	unsigned char *table;
	int *ovector;
	int ovecsize;
	char *nametable;
	int namecount;
	int nameentrysize;
}
object;

#define object_limit 256
static object *object_list[object_limit];
static int object_count = 0;

#define push_object(x) push_list ((void *) x, object_list, object_count)
#define check_object() check_list (object_count, object_limit)
#define is_object(x,y) in_list ((void *) x, (void **) object_list, &object_count, y)


/*
 * XC API
 */

gtm_char_t *
version (int argc UNUSED)
{
	return (char *) pcre_version ();
}

gtm_int_t
ofree (int argc, gtm_ulong_t ref)
{
	object *o = (object *) ref;
	check_argc (1);
	if (!is_object (o, 1))
		return E_INVREF;
	if (o -> extra != NULL)
		pcre_free_study (o -> extra);
	if (o -> table != NULL)
		pcre_free (o -> table);
	if (o -> ovector != NULL)
		pcre_free (o -> ovector);
	pcre_free (o);
	return 0;
}

gtm_int_t
compile (int argc,
	gtm_char_t *pattern, gtm_char_t *options_name, gtm_char_t *invalid_option /* [32] */,
	gtm_ulong_t *ref, gtm_char_t **err, gtm_int_t *erroffset, gtm_char_t *locale,
	gtm_ulong_t matchlimit, gtm_ulong_t recursionlimit)
{
	object *o;
	char *currlocale;
	int rc;
	int options;
	param options_param[] = {
		{ "ANCHORED",		PCRE_ANCHORED		},
		{ "CASELESS",		PCRE_CASELESS		},
		{ "DOLLAR_ENDONLY",	PCRE_DOLLAR_ENDONLY	},
		{ "DOTALL",		PCRE_DOTALL		},
		{ "EXTENDED",		PCRE_EXTENDED		},
		{ "FIRSTLINE",		PCRE_FIRSTLINE		},
		{ "MULTILINE",		PCRE_MULTILINE		},
		{ "NO_AUTO_CAPTURE",	PCRE_NO_AUTO_CAPTURE	},
		{ "DUPNAMES",		PCRE_DUPNAMES		},
		{ "UNGREEDY",		PCRE_UNGREEDY		},
		{ "BSR_ANYCRLF",	PCRE_BSR_ANYCRLF	},
		{ "BSR_UNICODE",	PCRE_BSR_UNICODE	},
		{ "JAVASCRIPT_COMPAT",	PCRE_JAVASCRIPT_COMPAT	},
		{ "NL_ANY",		PCRE_NEWLINE_ANY	},
		{ "NL_ANYCRLF",		PCRE_NEWLINE_ANYCRLF	},
		{ "NL_CR",		PCRE_NEWLINE_CR		},
		{ "NL_CRLF",		PCRE_NEWLINE_CRLF	},
		{ "NL_LF",		PCRE_NEWLINE_LF		},
		{ "UTF8",		PCRE_UTF8		},
		{ "UCP",		PCRE_UCP		},
		{ "NO_UTF8_CHECK",	PCRE_NO_UTF8_CHECK	}
	};
	check_argc (9);
	*ref = 0;
	*err = "";
	*erroffset = 0;
	invalid_option[0] = '\0';
	check_object ();
	check_param (get_flags (options));
	check_malloc (o = pcre_malloc (sizeof (*o)));
	memset (o, '\0', sizeof (*o));
	if (locale[0] != '\0')
	{
		currlocale = setlocale (LC_CTYPE, NULL);
		if (setlocale (LC_CTYPE, (strcasecmp (locale, "ENV") == 0 ? "" : locale)) == NULL)
		{
			free (o);
			return E_LOCALE;
		}
		o -> table = (unsigned char *) pcre_maketables ();
		setlocale (LC_CTYPE, currlocale);
	}
	if ((o -> re = pcre_compile (pattern, options, (const char **) err, erroffset, o -> table)) == NULL)
	{
		if (*err == NULL)
			*err = "";
		ofree (1, (gtm_ulong_t) o);
		return E_COMPILE;
	}
	o -> extra = pcre_study (o -> re, 0, (const char **) err);
	if (*err != NULL)
	{
		ofree (1, (gtm_ulong_t) o);
		return E_STUDY;
	}
	if ((matchlimit > 0) || (recursionlimit > 0))
	{
		if (o -> extra == NULL)
		{
			if ((o -> extra = malloc (sizeof (pcre_extra))) == NULL)
			{
				ofree (1, (gtm_ulong_t) o);
				return E_MALLOC;
			}
			memset (o -> extra, '\0', sizeof (pcre_extra));
		}
		if (matchlimit > 0)
		{
			o -> extra -> flags |= PCRE_EXTRA_MATCH_LIMIT;
			o -> extra -> match_limit = matchlimit;
		}
		if (recursionlimit > 0)
		{
			o -> extra -> flags |= PCRE_EXTRA_MATCH_LIMIT_RECURSION;
			o -> extra -> match_limit_recursion = recursionlimit;
		}
	}
	if ((rc = pcre_fullinfo (o -> re, o -> extra, PCRE_INFO_NAMECOUNT, &(o -> namecount))) != 0)
		return rc + E_BASE;
	if (o -> namecount > 0)
	{
		if ((rc = pcre_fullinfo (o -> re, o -> extra, PCRE_INFO_NAMEENTRYSIZE, &(o -> nameentrysize))) != 0)
			return rc + E_BASE;
		if ((rc = pcre_fullinfo (o -> re, o -> extra, PCRE_INFO_NAMETABLE, &(o -> nametable))) != 0)
			return rc + E_BASE;
	}
	*err = "";
	push_object (o);
	*ref = (gtm_long_t) o;
	return 0;
}

gtm_int_t
exec (int argc,
	gtm_ulong_t ref, gtm_string_t *subject, gtm_int_t length, gtm_int_t startoffset,
	gtm_char_t *options_name, gtm_char_t *invalid_option /* [32] */)
{
	object *o = (object *) ref;
	int options;
	int rc;
	param options_param[] = {
		{ "ANCHORED",		PCRE_ANCHORED		},
		{ "BSR_ANYCRLF",	PCRE_BSR_ANYCRLF	},
		{ "BSR_UNICODE",	PCRE_BSR_UNICODE	},
		{ "NL_ANY",		PCRE_NEWLINE_ANY	},
		{ "NL_ANYCRLF",		PCRE_NEWLINE_ANYCRLF	},
		{ "NL_CR",		PCRE_NEWLINE_CR		},
		{ "NL_CRLF",		PCRE_NEWLINE_CRLF	},
		{ "NL_LF",		PCRE_NEWLINE_LF		},
		{ "NOTBOL",		PCRE_NOTBOL		},
		{ "NOTEOL",		PCRE_NOTEOL		},
		{ "NOTEMPTY",		PCRE_NOTEMPTY		},
		{ "NOTEMPTY_ATSTART",	PCRE_NOTEMPTY_ATSTART	},
		{ "NO_START_OPTIMIZE",	PCRE_NO_START_OPTIMIZE	},
		{ "NO_UTF8_CHECK",	PCRE_NO_UTF8_CHECK	},
		{ "PARTIAL_SOFT",	PCRE_PARTIAL_SOFT	},
		{ "PARTIAL_HARD",	PCRE_PARTIAL_HARD	}
	};
	check_argc (6);
	invalid_option[0] = '\0';
	check_param (get_flags (options));
	if (!is_object (o, 0))
		return E_INVREF;
	if (length > subject -> length)
		return E_LENGTH;
	if ((rc = pcre_fullinfo (o -> re, o -> extra, PCRE_INFO_CAPTURECOUNT, &(o -> ovecsize))) != 0)
	{
		return rc + E_BASE;
	}
	o -> ovecsize = (o -> ovecsize + 1) * 3;
	if ((o -> ovector = pcre_malloc (o -> ovecsize * sizeof (o -> ovector[0]))) == NULL)
	{
		ofree (1, (gtm_ulong_t) o);
		return E_MALLOC;
	}
	if ((rc = pcre_exec (o -> re, o -> extra, subject -> address, length, startoffset, options, o -> ovector, o -> ovecsize)) > 0)
		return rc;
	switch (rc)
	{
		case 0:
			/*
			 * ovector array was too small and its size can't be
			 * controlled from the M world, so it's an exception
			 * there
			 */
			return E_INTBUF;

		case PCRE_ERROR_NOMATCH:
			/*
			 * NOMATCH is so common, that is should not raise any exception
			 */
			return 0;

		default:
			/*
			 * standard code range is remapped so as not to lead
			 * to a confusion
			 */
			return rc + E_BASE;
	}
}

gtm_int_t
ovector (int argc, gtm_ulong_t ref, gtm_int_t i, gtm_int_t *n)
{
	object *o = (object *) ref;
	check_argc (3);
	*n = 0;
	if ((!is_object (o, 0)) || (o -> ovector == NULL) || (i >= o -> ovecsize))
		return E_INVREF;
	*n = o -> ovector[i];
	return 0;
}

gtm_int_t
ovecsize (int argc, gtm_ulong_t ref, gtm_int_t *n)
{
	object *o = (object *) ref;
	check_argc (2);
	*n = 0;
	if ((!is_object (o, 0)) || (o -> ovector == NULL))
		return E_INVREF;
	*n = o -> ovecsize;
	return 0;
}

gtm_int_t
stackusage (int argc UNUSED)
{
	return pcre_exec (NULL, NULL, NULL, -999, -999, 0, NULL, 0);
}

gtm_int_t
fullinfo (int argc,
	gtm_ulong_t ref, gtm_char_t *options_name, gtm_char_t *invalid_option /* [32] */,
	gtm_int_t *is_string, gtm_string_t *s /* [1024] */, gtm_long_t *n)
{
	object *o = (object *) ref;
	int options;
	int rc;
	int i;
	int seen;
	char *p;
	ssize_t l;
	/* buffers */
	int rsint;
	unsigned long rulong;
	char *rchar;
	param options_param[] = {
		{ "OPTIONS",		PCRE_INFO_OPTIONS		},
		{ "SIZE",		PCRE_INFO_SIZE			},
		{ "CAPTURECOUNT",	PCRE_INFO_CAPTURECOUNT		},
		{ "BACKREFMAX",		PCRE_INFO_BACKREFMAX		},
		{ "FIRSTBYTE",		PCRE_INFO_FIRSTBYTE		},
		{ "FIRSTTABLE",		PCRE_INFO_FIRSTTABLE		},
		{ "LASTLITERAL",	PCRE_INFO_LASTLITERAL		},
		{ "NAMEENTRYSIZE",	PCRE_INFO_NAMEENTRYSIZE		},
		{ "NAMECOUNT",		PCRE_INFO_NAMECOUNT		},

	/*
	 *	see nametable() for accessing PCRE_INFO_NAMETABLE data
	 *	{ "NAMETABLE",		PCRE_INFO_NAMETABLE		},
	 */

		{ "STUDYSIZE",		PCRE_INFO_STUDYSIZE		},

	/*
	 *	these are undocumented PCRE internals, they are useless
	 *	outside of C code
	 *
	 *	{ "DEFAULT_TABLES",	PCRE_INFO_DEFAULT_TABLES	},
	 */

		{ "OKPARTIAL",		PCRE_INFO_OKPARTIAL		},
		{ "JCHANGED",		PCRE_INFO_JCHANGED		},
		{ "HASCRORLF",		PCRE_INFO_HASCRORLF		},
		{ "MINLENGTH",		PCRE_INFO_MINLENGTH		},
		{ "JIT",		PCRE_INFO_JIT			},
		{ "JITSIZE",		PCRE_INFO_JITSIZE		}
	};
	param names[] = {
		/*
		 * matched bits are cleared so as to enable clear reporting
		 * for "NL_CRLF" which otherwise would be "NL_CR|NL_LF",
		 * and "NL_ANYCRLF" which is a complex bit map
		 */

		/*
		 * complex bit maps should be ordered first, "NL_ANYCRLF"
		 * is more possessive than "NL_CRLF", so it goes first
		 */
	 	{ "NL_CRLF",		PCRE_NEWLINE_CRLF		},
	 	{ "NL_ANYCRLF",		PCRE_NEWLINE_ANYCRLF		},

		{ "CASELESS",		PCRE_CASELESS			},
		{ "MULTILINE",		PCRE_MULTILINE			},
		{ "DOTALL",		PCRE_DOTALL			},
		{ "EXTENDED",		PCRE_EXTENDED			},
		{ "ANCHORED",		PCRE_ANCHORED			},
		{ "DOLLAR_ENDONLY",	PCRE_DOLLAR_ENDONLY		},
		{ "EXTRA",		PCRE_EXTRA			},
		{ "NOTBOL",		PCRE_NOTBOL			},
		{ "NOTEOL",		PCRE_NOTEOL			},
		{ "UNGREEDY",		PCRE_UNGREEDY			},
		{ "NOTEMPTY",		PCRE_NOTEMPTY			},
		{ "UTF8",		PCRE_UTF8			},
		{ "NO_AUTO_CAPTURE",	PCRE_NO_AUTO_CAPTURE		},
		{ "NO_UTF8_CHECK",	PCRE_NO_UTF8_CHECK		},
		{ "AUTO_CALLOUT",	PCRE_AUTO_CALLOUT		},
		{ "PARTIAL_SOFT",	PCRE_PARTIAL_SOFT		},
		{ "DFA_SHORTEST",	PCRE_DFA_SHORTEST		},
		{ "DFA_RESTART",	PCRE_DFA_RESTART		},
		{ "FIRSTLINE",		PCRE_FIRSTLINE			},
		{ "DUPNAMES",		PCRE_DUPNAMES			},
		{ "NL_CR",		PCRE_NEWLINE_CR			},
		{ "NL_LF",		PCRE_NEWLINE_LF			},
		{ "NL_ANY",		PCRE_NEWLINE_ANY		},
		{ "BSR_ANYCRLF",	PCRE_BSR_ANYCRLF		},
		{ "BSR_UNICODE",	PCRE_BSR_UNICODE		},
		{ "JAVASCRIPT_COMPAT",	PCRE_JAVASCRIPT_COMPAT		},
		{ "NO_START_OPTIMIZE",	PCRE_NO_START_OPTIMIZE		},
		{ "PARTIAL_HARD",	PCRE_PARTIAL_HARD		},
		{ "NOTEMPTY_ATSTART",	PCRE_NOTEMPTY_ATSTART		},
		{ "UCP",		PCRE_UCP			}
	};
	check_argc (6);
	*is_string = 0;
	invalid_option[0] = '\0';
	s -> length = 0;
	*n = 0;
	check_param (get_param (options));
	if (!is_object (o, 0))
		return E_INVREF;
	switch (options)
	{
		/*
		 * int
		 */
		case PCRE_INFO_BACKREFMAX:
		case PCRE_INFO_CAPTURECOUNT:
		case PCRE_INFO_FIRSTBYTE:
		case PCRE_INFO_HASCRORLF:
		case PCRE_INFO_JCHANGED:
		case PCRE_INFO_JIT:
		case PCRE_INFO_LASTLITERAL:
		case PCRE_INFO_MINLENGTH:
		case PCRE_INFO_NAMEENTRYSIZE:
		case PCRE_INFO_NAMECOUNT:
		case PCRE_INFO_OKPARTIAL:
			if ((rc = pcre_fullinfo (o -> re, o -> extra, options, &rsint)) == 0)
				*n = (long) rsint;
			break;

		/*
		 * size_t
		 */
		case PCRE_INFO_JITSIZE:
		case PCRE_INFO_SIZE:
		case PCRE_INFO_STUDYSIZE:
			if ((rc = pcre_fullinfo (o -> re, o -> extra, options, &rulong)) == 0)
				/* should never exceed long range */
				*n = (long) rulong;
			break;

		/*
		 * unsigned long (stringified)
		 */
		case PCRE_INFO_OPTIONS:
			if ((rc = pcre_fullinfo (o -> re, o -> extra, options, &rsint)) == 0)
			{
				*is_string = 1;
				p = s -> address;
				l = 1024;
				seen = 0;
				for (i = 0; i < sizeof (names) / sizeof (*names); i++)
					if ((rsint & names[i].value) == names[i].value)
					{
						rsint &= ~names[i].value;
						if (seen)
						{
							if ((p = strpncopy (p, list_delimiter, &l)) == NULL)
							{
								s -> length = 1024 - l;
								return E_INTBUF;
							}
						}
						else
							seen++;
						if ((p = strpncopy (p, names[i].name, &l)) == NULL)
						{
							s -> length = 1024 - l;
							return E_INTBUF;
						}
					}
				s -> length = 1024 - l;
			}
			break;

		/*
		 * unsigned char *
		 */

	/*
	 *	unsupported, see comments above
	 *	case PCRE_INFO_DEFAULT_TABLES:
	 */
		case PCRE_INFO_FIRSTTABLE:
			if ((rc = pcre_fullinfo (o -> re, o -> extra, options, &rchar)) == 0)
			{
				if (rchar != NULL)
				{
					memcpy (s -> address, rchar, 8);
					s -> length = 8;
				}
				*is_string = 1;
			}
			break;

	/*
	 *	unsupported, see comments above
	 *	case PCRE_INFO_NAMETABLE:
	 */

		default:
			/*
			 * should never happen
			 */
			return E_OPTNAME;
	}
	if (rc != 0)
		return rc + E_BASE;
	return 0;
}

gtm_int_t
nametable (int argc, gtm_ulong_t ref, gtm_int_t i, gtm_int_t *n, gtm_char_t *s /* [64] */)
{
	object *o = (object *) ref;
	check_argc (4);
	*n = 0;
	s[0] = '\0';
	if (!is_object (o, 0))
		return E_INVREF;
	/*
	 * use i in MUMPS-ish way, that is beginning with 1,
	 */
	if ((i < 1) || (i > o -> namecount))
		return 0;
	i--;
	*n = o -> nametable[o -> nameentrysize * i] * 256 +
		o -> nametable[o -> nameentrysize * i + 1];
	return strncopy (s, &(o -> nametable[o -> nameentrysize * i + 2]), 64);
}

gtm_int_t
config (int argc,
	gtm_char_t *options_name, gtm_char_t *invalid_option /* [32] */,
	gtm_int_t *is_string, gtm_char_t *s /* [1024] */, gtm_long_t *n)
{
	int options;
	int rc;
	int i;
	/* buffers */
	char *rchar;
	int rsint;
	long rslong;
	param options_param[] = {
		{ "UTF8",			PCRE_CONFIG_UTF8			},
		{ "NEWLINE",			PCRE_CONFIG_NEWLINE			},
		{ "LINK_SIZE",			PCRE_CONFIG_LINK_SIZE			},
		{ "POSIX_MALLOC_THRESHOLD",	PCRE_CONFIG_POSIX_MALLOC_THRESHOLD	},
		{ "MATCH_LIMIT",		PCRE_CONFIG_MATCH_LIMIT			},
		{ "STACKRECURSE",		PCRE_CONFIG_STACKRECURSE		},
		{ "UNICODE_PROPERTIES",		PCRE_CONFIG_UNICODE_PROPERTIES		},
		{ "MATCH_LIMIT_RECURSION",	PCRE_CONFIG_MATCH_LIMIT_RECURSION	},
		{ "BSR",			PCRE_CONFIG_BSR				},
		{ "JIT",			PCRE_CONFIG_JIT				},
		{ "JITTARGET",			PCRE_CONFIG_JITTARGET			}
	};
	param names[] = {
		{ "NL_LF",	10	},
		{ "NL_CR",	13	},
		{ "NL_CRLF",	3338	},
		{ "NL_ANY",	-1	},
		{ "NL_ANYCRLF",	-2	}
	};
	check_argc (5);
	*is_string = 0;
	s[0] = '\0';
	invalid_option[0] = '\0';
	*n = 0;
	check_param (get_param (options));
	switch (options)
	{
		/*
		 * int
		 */
		case PCRE_CONFIG_UTF8:
		case PCRE_CONFIG_UNICODE_PROPERTIES:
		case PCRE_CONFIG_JIT:
		case PCRE_CONFIG_BSR:
		case PCRE_CONFIG_LINK_SIZE:
		case PCRE_CONFIG_POSIX_MALLOC_THRESHOLD:
		case PCRE_CONFIG_STACKRECURSE:
			if ((rc = pcre_config (options, &rsint)) == 0)
				*n = (long) rsint;
			break;

		/*
		 * int (stringified)
		 */
		case PCRE_CONFIG_NEWLINE:
			if ((rc = pcre_config (options, &rsint)) == 0)
			{
				for (i = 0; i < sizeof (names) / sizeof (*names); i++)
					if (rsint == names[i].value)
					{
						if (strncopy (s, names[i].name, 1024) != 0)
							return E_INTBUF;
						break;
					}
				*is_string = 1;
			}
			break;

		/*
		 * long
		 */
		case PCRE_CONFIG_MATCH_LIMIT:
		case PCRE_CONFIG_MATCH_LIMIT_RECURSION:
			if ((rc = pcre_config (options, &rslong)) == 0)
				*n = rslong;
			break;

		/*
		 * char *
		 */
		case PCRE_CONFIG_JITTARGET:
			if ((rc = pcre_config (options, &rchar)) == 0)
			{
				if (rchar != NULL)
				{
					if (strncopy (s, rchar, 1024) != 0)
						return E_INTBUF;
				}
				*is_string = 1;
			}
			break;

		default:
			/*
			 * should never happen
			 */
			return E_OPTNAME;
	}
	if (rc != 0)
		return rc + E_BASE;
	return 0;
}

