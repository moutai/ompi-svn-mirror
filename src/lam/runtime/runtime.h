/*
 * $HEADER$
 */

#ifndef LAM_RUNTIME_H
#define LAM_RUNTIME_H

#include "lam_config.h"


#ifdef __cplusplus
extern "C" {
#endif

  int lam_abort(int status, char *fmt, ...);
  int lam_init(int argc, char* argv[]);
  int lam_finalize(void);
  int lam_rte_init(bool *allow_multi_user_threads, bool *have_hidden_threads);
  int lam_rte_finalize(void);

#ifdef __cplusplus
}
#endif

#endif /* LAM_RUNTIME_H */
