#include <string.h>

/* ts A61008 */
/* #include <a ssert.h> */

#include <stdlib.h>
#include "../version.h"
#include "util.h"
#include "libburn.h"

char *burn_strdup(char *s)
{
	char *ret;
	int l;

	/* ts A61008 */
	/* a ssert(s); */
	if (s == NULL)
		return NULL;

	l = strlen(s) + 1;
	ret = malloc(l);
	memcpy(ret, s, l);

	return ret;
}

char *burn_strndup(char *s, int n)
{
	char *ret;
	int l;

	/* ts A61008 */
	/* a ssert(s); */
	/* a ssert(n > 0); */
	if (s == NULL || n <= 0)
		return NULL;

	l = strlen(s);
	ret = malloc(l < n ? l : n);

	memcpy(ret, s, l < n - 1 ? l : n - 1);
	ret[n - 1] = '\0';

	return ret;
}

void burn_version(int *major, int *minor, int *micro)
{
    *major = BURN_MAJOR_VERSION;
    *minor = BURN_MINOR_VERSION;
    *micro = BURN_MICRO_VERSION;
}
