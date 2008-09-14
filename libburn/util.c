#include <string.h>

/* ts A61008 */
/* #include <a ssert.h> */

#include <stdlib.h>

/* ts A80914 : This is unneeded. Version info comes from libburn.h.
#include "v ersion.h"
*/

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
/* ts A80408 : switched from configure.ac versioning to libburn.h versioning */
	*major = burn_header_version_major;
	*minor = burn_header_version_minor;
	*micro = burn_header_version_micro;
}
