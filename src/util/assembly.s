/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Trustees of the University of Tennessee.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

/*
 * File to instantiate assembly level code for non-GNU C compilers.
 */

#ifndef __GNUC__

#if   defined(__alpha__)
# include "include/sys/alpha/atomic.s"
#elif defined(__amd64__)
# include "include/sys/amd64/atomic.s"
#elif defined(__i386__)
# include "include/sys/ia32/atomic.s"
#elif defined(__ia64__)
# include "include/sys/ia64/atomic.s"
#elif defined(__POWERPC__)
# include "include/sys/powerpc/atomic.s"
#elif defined(__sparc__) || defined(__sparc)
# include "include/sys/sparc64/atomic.s"
#endif

#endif
