/**
 * VampirTrace
 * http://www.tu-dresden.de/zih/vampirtrace
 *
 * Copyright (c) 2005-2012, ZIH, TU Dresden, Federal Republic of Germany
 *
 * Copyright (c) 1998-2005, Forschungszentrum Juelich, Juelich Supercomputing
 *                          Centre, Federal Republic of Germany
 *
 * See the file COPYING in the package base directory for details
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vt_comp.h"
#include "vt_defs.h"
#include "vt_memhook.h"
#include "vt_pform.h"
#include "vt_thrd.h"
#include "vt_trc.h"

/*
 *-----------------------------------------------------------------------------
 * Simple hash table to map function data to region identifier
 *-----------------------------------------------------------------------------
 */

typedef struct HN {
  long id;            /* hash code (address of function name) */
  uint32_t vtid;      /* associated region identifier  */
  char* func;
  char* file;
  int lno;
  struct HN* next;
} HashNode;

#define HASH_MAX 1021

static int xl_init = 1;       /* is initialization needed? */

static HashNode* htab[HASH_MAX];

/*
 * Stores region identifier `e' under hash code `h'
 */

static HashNode *hash_put(long h, uint32_t e) {
  long id = h % HASH_MAX;
  HashNode *add = (HashNode*)malloc(sizeof(HashNode));
  add->id = h;
  add->vtid = e;
  add->next = htab[id];
  htab[id] = add;
  return add;
}

/*
 * Lookup hash code `h'
 * Returns pointer to function data if already stored, otherwise 0
 */

static HashNode *hash_get(long h) {
  long id = h % HASH_MAX;
  HashNode *curr = htab[id];
  while ( curr ) {
    if ( curr->id == h ) {
      return curr;
    }
    curr = curr->next;
  }
  return 0;
}

/*
 * Register new region
 */

static HashNode *register_region(char *func, char *file, int lno) {
  uint32_t rid;
  uint32_t fid;
  HashNode* nhn;

  /* -- register file and region and store region identifier -- */
  fid = vt_def_scl_file(VT_CURRENT_THREAD, file);
  rid = vt_def_region(VT_CURRENT_THREAD, func, fid, lno, VT_NO_LNO, NULL,
                      VT_FUNCTION);
  nhn = hash_put((long) func, rid);
  nhn->func = func;
  nhn->file = file;
  nhn->lno  = lno;
  return nhn;
}

void xl_finalize(void);
void __func_trace_enter(char* name, char* fname, int lno);
void __func_trace_exit(char* name, char* fname, int lno);

/*
 * Finalize instrumentation interface
 */

void xl_finalize()
{
  int i;

  for ( i = 0; i < HASH_MAX; i++ )
  {
    if ( htab[i] ) {
      free(htab[i]);
      htab[i] = NULL;
    }
  }
  xl_init = 1;
}

/*
 * This function is called at the entry of each function
 * The call is generated by the IBM xl compilers
 */

void __func_trace_enter(char* name, char* fname, int lno) {
  HashNode *hn;
  uint64_t time;

  /* -- if not yet initialized, initialize VampirTrace -- */
  if ( xl_init ) {
    VT_MEMHOOKS_OFF();
    xl_init = 0;
    vt_open();
    vt_comp_finalize = &xl_finalize;
    VT_MEMHOOKS_ON();
  }

  /* -- if VampirTrace already finalized, return -- */
  if ( !vt_is_alive ) return;

  /* -- ignore IBM OMP runtime functions -- */
  if ( strchr(name, '@') != NULL ) return;

  VT_MEMHOOKS_OFF();

  time = vt_pform_wtime();

  /* -- get region identifier -- */
  if ( (hn = hash_get((long) name)) == 0 ) {
    /* -- region entered the first time, register region -- */
#if (defined(VT_MT) || defined(VT_HYB))
    VTTHRD_LOCK_IDS();
    if ( (hn = hash_get((long) name)) == 0 )
      hn = register_region(name, fname, lno);
    VTTHRD_UNLOCK_IDS();
#else /* VT_MT || VT_HYB */
    hn = register_region(name, fname, lno);
#endif /* VT_MT || VT_HYB */
  }

  /* -- write enter record -- */
  vt_enter(VT_CURRENT_THREAD, &time, hn->vtid);

  VT_MEMHOOKS_ON();
}

/*
 * This function is called at the exit of each function
 * The call is generated by the IBM xl compilers
 */

void __func_trace_exit(char* name, char *fname, int lno) {
  uint64_t time;

  /* -- if VampirTrace already finalized, return -- */
  if ( !vt_is_alive ) return;

  VT_MEMHOOKS_OFF();

  /* -- ignore IBM OMP runtime functions -- */
  if ( strchr(name, '@') != NULL )
  {
    VT_MEMHOOKS_ON();
    return;
  }

  time = vt_pform_wtime();

  /* -- write exit record -- */
  if ( hash_get((long) name) ) {
    vt_exit(VT_CURRENT_THREAD, &time);
  }

  VT_MEMHOOKS_ON();
}
