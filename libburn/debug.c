/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifdef WIN32
#include <windows.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include "libburn.h"
#include "debug.h"

static int burn_verbosity = 0;

void burn_set_verbosity(int v)
{
	burn_verbosity = v;
}

void burn_print(int level, const char *a, ...)
{
#ifdef WIN32
	char debug_string_data[256];
#endif
	va_list vl;

	if (level <= burn_verbosity) {
		va_start(vl, a);
#ifdef WIN32
		vsprintf(debug_string_data, a, vl);
		OutputDebugString(debug_string_data);
#else
		vfprintf(stderr, a, vl);
#endif
	}
}
