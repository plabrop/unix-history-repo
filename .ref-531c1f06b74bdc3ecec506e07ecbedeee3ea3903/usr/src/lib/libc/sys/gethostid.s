/* gethostid.s 4.1 82/12/04 */

#include "SYS.h"

SYSCALL(gethostid)
	ret		# hostid = gethostid()
