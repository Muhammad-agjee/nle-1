/* Copyright (C) 2001 by Alex Kompel <shurikk@pacbell.net> */
/* Copyright (c) NetHack PC Development Team 1993, 1994.  */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef WCECONF_H
#define WCECONF_H

#pragma warning(disable:4142) /* benign redefinition of type */

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>

/* #define SHELL	/* nt use of pcsys routines caused a hang */

#define RANDOM		/* have Berkeley random(3) */
#define TEXTCOLOR	/* Color text */

#define EXEPATH			/* Allow .exe location to be used as HACKDIR */
#define TRADITIONAL_GLYPHMAP	/* Store glyph mappings at level change time */

#define PC_LOCKING		/* Prevent overwrites of aborted or in-progress games */
				/* without first receiving confirmation. */

#define NOTSTDC		/* no strerror() */

/*
 * -----------------------------------------------------------------
 *  The remaining code shouldn't need modification.
 * -----------------------------------------------------------------
 */
/* #define SHORT_FILENAMES	/* All NT filesystems support long names now */

#ifdef MICRO
#undef MICRO			/* never define this! */
#endif

#define NOCWD_ASSUMPTIONS	/* Always define this. There are assumptions that
                                   it is defined for WIN32.
				   Allow paths to be specified for HACKDIR,
				   LEVELDIR, SAVEDIR, BONESDIR, DATADIR,
				   SCOREDIR, LOCKDIR, CONFIGDIR, and TROUBLEDIR */
#define NO_TERMS
#define ASCIIGRAPH

#ifdef OPTIONS_USED
#undef OPTIONS_USED
#endif
#ifdef MSWIN_GRAPHICS
#define OPTIONS_USED	"guioptions"
#else
#define OPTIONS_USED	"ttyoptions"
#endif
#define OPTIONS_FILE OPTIONS_USED

#define PORT_HELP	"porthelp"

#include <string.h>	/* Provides prototypes of strncmpi(), etc.     */
#ifdef STRNCMPI
#define strncmpi(a,b,c) _strnicmp(a,b,c)
#endif

#ifdef STRCMPI
#define strcmpi(a,b) _stricmp(a,b)
#define stricmp(a,b) _stricmp(a,b)
#endif

#include <stdlib.h>

#define PATHLEN		BUFSZ /* maximum pathlength */
#define FILENAME	BUFSZ /* maximum filename length (conservative) */

#if defined(_MAX_PATH) && defined(_MAX_FNAME)
# if (_MAX_PATH < BUFSZ) && (_MAX_FNAME < BUFSZ)
#undef PATHLEN
#undef FILENAME
#define PATHLEN		_MAX_PATH
#define FILENAME	_MAX_FNAME
# endif
#endif


#define NO_SIGNAL
#define index	strchr
#define rindex	strrchr
#define USE_STDARG
#ifdef RANDOM
/* Use the high quality random number routines. */
#define Rand()	random()
#else
#define Rand()	rand()
#endif

#define FCMASK	0660	/* file creation mask */
#define regularize	nt_regularize
#define HLOCK "NHPERM"

#ifndef M
#define M(c)		((char) (0x80 | (c)))
/* #define M(c)		((c) - 128) */
#endif

#ifndef C
#define C(c)		(0x1f & (c))
#endif

#if defined(DLB)
#define FILENAME_CMP  _stricmp		      /* case insensitive */
#endif

#if 0
extern char levels[], bones[], permbones[],
#endif /* 0 */

/* this was part of the MICRO stuff in the past */
extern const char *alllevels, *allbones;
extern char hackdir[];
#define ABORT C('a')
#define getuid() 1
#define getlogin() ((char *)0)
extern void NDECL(win32_abort);
#ifdef WIN32CON
extern void FDECL(nttty_preference_update, (const char *));
extern void NDECL(toggle_mouse_support);
#endif

#ifndef alloca
#define ALLOCA_HACK	/* used in util/panic.c */
#endif

#ifndef REDO
#undef	Getchar
#define Getchar nhgetch
#endif

#ifdef _MSC_VER
#if 0
#pragma warning(disable:4018)	/* signed/unsigned mismatch */
#pragma warning(disable:4305)	/* init, conv from 'const int' to 'char' */
#endif
#pragma warning(disable:4761)	/* integral size mismatch in arg; conv supp*/
#ifdef YYPREFIX
#pragma warning(disable:4102)	/* unreferenced label */
#endif
#endif

/* Detect HPC - CE ver 2.11 */
#if UNDER_CE<300
#define WIN_CE_2xx
#endif

/* UNICODE stuff */
#define NHSTR_BUFSIZE	255
#ifdef UNICODE
	#define NH_W2A(w, a, cb)     ( WideCharToMultiByte(                              \
												   CP_ACP,                      \
												   0,                           \
												   (w),                           \
												   -1,                          \
												   (a),                           \
												   (cb),                          \
												   NULL,                        \
												   NULL), (a) )

	#define NH_A2W(a, w, cb)     ( MultiByteToWideChar(                              \
												   CP_ACP,                      \
												   0,                           \
												   (a),                           \
												   -1,                          \
												   (w),                           \
												   (cb)), (w) )
#else
	#define NH_W2A(w, a, cb)     (strncpy((a), (w), (cb)))

	#define NH_A2W(a, w, cb)     (strncpy((w), (a), (cb)))
#endif

extern int FDECL(set_win32_option, (const char *, const char *));

/* Missing definitions */
extern int		mswin_have_input();
#define kbhit	mswin_have_input

#define getenv(a) ((char*)NULL)

/* __stdio.h__ */
#define perror(a)
#define freopen(a, b, c) fopen(a, b)
extern int isatty(int);

/* __time.h___ */
#ifndef _TIME_T_DEFINED
typedef __int64 time_t;        /* time value */
#define _TIME_T_DEFINED     /* avoid multiple def's of time_t */
#endif

#ifndef _TM_DEFINED
struct tm {
        int tm_sec;     /* seconds after the minute - [0,59] */
        int tm_min;     /* minutes after the hour - [0,59] */
        int tm_hour;    /* hours since midnight - [0,23] */
        int tm_mday;    /* day of the month - [1,31] */
        int tm_mon;     /* months since January - [0,11] */
        int tm_year;    /* years since 1900 */
        int tm_wday;    /* days since Sunday - [0,6] */
        int tm_yday;    /* days since January 1 - [0,365] */
        int tm_isdst;   /* daylight savings time flag - - NOT IMPLEMENTED */
        };
#define _TM_DEFINED
#endif

struct tm * __cdecl localtime(const time_t *);
time_t __cdecl time(time_t *);

/* __stdio.h__ */
#ifndef BUFSIZ
#define BUFSIZ 255
#endif

#define rewind(stream) (void)fseek( stream, 0L, SEEK_SET )

/* __io.h__ */
typedef long off_t;

int __cdecl close(int);
int __cdecl creat(const char *, int);
int __cdecl eof(int);
long __cdecl lseek(int, long, int);
int __cdecl open(const char *, int, ...);
int __cdecl read(int, void *, unsigned int);
int __cdecl unlink(const char *);
int __cdecl write(int, const void *, unsigned int);
int __cdecl rename(const char *, const char *);

#ifdef DeleteFile
#undef DeleteFile
#endif
#define DeleteFile(a) unlink(a)

int chdir( const char *dirname );
char *getcwd( char *buffer, int maxlen );

/* __stdlib.h__ */
#define abort()  (void)TerminateProcess(GetCurrentProcess(), 0)

/* sys/stat.h */
#define S_IWRITE  GENERIC_WRITE
#define S_IREAD   GENERIC_READ


/* CE 2.xx is missing even more stuff */
#ifdef WIN_CE_2xx
#define ZeroMemory(p, s)         memset((p), 0, (s))

int __cdecl isupper(int c);
int __cdecl isdigit(int c);
int __cdecl isspace(int c);
int __cdecl isprint(int c);

char* __cdecl _strdup(const char* s);
char* __cdecl strrchr( const char *string, int c );
int   __cdecl _stricmp(const char* a, const char* b);
#endif

/* ARM - the processor; avoids conflict with ARM in hack.h */
# ifdef ARM
# undef ARM
# endif

#endif /* WCECONF_H */
