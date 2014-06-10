#ifndef __UTIL
#define __UTIL

/* for struct stat */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* ts A90905 */
int burn_util_make_printable_word(char **text, int flag);

/* ts B11216 */
char *burn_sfile_fgets(char *line, int maxl, FILE *fp);
char *burn_printify(char *msg);

/* ts B30521 */
void burn_int_to_lsb(int val, char *target);

/* ts B30609 */
double burn_get_time(int flag);

/* ts B40609 */
off_t burn_sparse_file_addsize(off_t write_start, struct stat *stbuf);

#endif
