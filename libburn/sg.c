
/* ts A61013 : It would be nice if autotools could do that job */

#ifdef __FreeBSD__

#include "sg-freebsd.c"

#else

#include "sg-linux.c"

#endif

