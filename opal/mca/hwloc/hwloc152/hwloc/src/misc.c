/*
 * Copyright © 2009 CNRS
 * Copyright © 2009-2011 inria.  All rights reserved.
 * Copyright © 2009-2010 Université Bordeaux 1
 * Copyright © 2009-2011 Cisco Systems, Inc.  All rights reserved.
 * See COPYING in top-level directory.
 */

#include <private/autogen/config.h>
#include <private/misc.h>

#include <stdarg.h>
#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

int hwloc_snprintf(char *str, size_t size, const char *format, ...)
{
  int ret;
  va_list ap;
  static char bin;
  size_t fakesize;
  char *fakestr;

  /* Some systems crash on str == NULL */
  if (!size) {
    str = &bin;
    size = 1;
  }

  va_start(ap, format);
  ret = vsnprintf(str, size, format, ap);
  va_end(ap);

  if (ret >= 0 && (size_t) ret != size-1)
    return ret;

  /* vsnprintf returned size-1 or -1. That could be a system which reports the
   * written data and not the actually required room. Try increasing buffer
   * size to get the latter. */

  fakesize = size;
  fakestr = NULL;
  do {
    fakesize *= 2;
    free(fakestr);
    fakestr = malloc(fakesize);
    if (NULL == fakestr)
      return -1;
    va_start(ap, format);
    errno = 0;
    ret = vsnprintf(fakestr, fakesize, format, ap);
    va_end(ap);
  } while ((size_t) ret == fakesize-1 || (ret < 0 && (!errno || errno == ERANGE)));

  if (ret >= 0 && size) {
    if (size > (size_t) ret+1)
      size = ret+1;
    memcpy(str, fakestr, size-1);
    str[size-1] = 0;
  }
  free(fakestr);

  return ret;
}

int hwloc_namecoloncmp(const char *haystack, const char *needle, size_t n)
{
  size_t i = 0;
  while (*haystack && *haystack != ':') {
    int ha = *haystack++;
    int low_h = tolower(ha);
    int ne = *needle++;
    int low_n = tolower(ne);
    if (low_h != low_n)
      return 1;
    i++;
  }
  return i < n;
}

void hwloc_add_uname_info(struct hwloc_topology *topology __hwloc_attribute_unused)
{
#ifdef HAVE_UNAME
  struct utsname utsname;

  if (uname(&utsname) < 0)
    return;

  hwloc_obj_add_info(topology->levels[0][0], "OSName", utsname.sysname);
  hwloc_obj_add_info(topology->levels[0][0], "OSRelease", utsname.release);
  hwloc_obj_add_info(topology->levels[0][0], "OSVersion", utsname.version);
  hwloc_obj_add_info(topology->levels[0][0], "HostName", utsname.nodename);
  hwloc_obj_add_info(topology->levels[0][0], "Architecture", utsname.machine);
#endif /* HAVE_UNAME */
}
