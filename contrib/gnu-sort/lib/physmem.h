#ifndef PHYSMEM_H_
# define PHYSMEM_H_ 1

# if HAVE_CONFIG_H
#  include <config.h>
# endif

# ifndef PARAMS
#  if defined PROTOTYPES || (defined __STDC__ && __STDC__)
#   define PARAMS(Args) Args
#  else
#   define PARAMS(Args) ()
#  endif
# endif

double physmem_total PARAMS ((void));
double physmem_available PARAMS ((void));

#endif /* PHYSMEM_H_ */
