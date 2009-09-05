#ifndef __UTIL
#define __UTIL

char *burn_strdup(char *s);

char *burn_strndup(char *s, int n);

/* ts A90905 */
int burn_util_make_printable_word(char **text, int flag);

#endif
