/*
 * $HEADER$
 */
/**
 * @file
 */
#ifndef MCA_ALLOCATOR_BASE_H
#define MCA_ALLOCATOR_BASE_H

#include "ompi_config.h"

#include "class/ompi_list.h"
#include "mca/mca.h"
#include "mca/allocator/allocator.h"


struct mca_allocator_base_selected_module_t {
  ompi_list_item_t super;
  mca_allocator_base_module_t *apsm_module;
  mca_allocator_t *absm_actions;
};
typedef struct mca_allocator_base_selected_module_t mca_allocator_base_selected_module_t;


/*
 * Global functions for MCA: overall PTL open and close
 */

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif
  int mca_allocator_base_open(void);
  int mca_allocator_base_select(bool *allow_multi_user_threads);
  int mca_allocator_base_close(void);
#if defined(c_plusplus) || defined(__cplusplus)
}
#endif


/*
 * Globals
 */


extern int mca_allocator_base_output;
extern ompi_list_t mca_allocator_base_modules_available;
extern ompi_list_t mca_allocator_base_modules_initialized;

#endif /* MCA_ALLOCATOR_BASE_H */
