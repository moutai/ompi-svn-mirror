/**
  * $HEADER$
  */
/**
  * @file
  */
#ifndef MCA_ALLOCATOR_H
#define MCA_ALLOCATOR_H
#include "mca/mca.h"

struct mca_allocator_t;

/**
  * allocate function typedef
  */
typedef void* (*mca_allocator_alloc_fn_t)(struct mca_allocator_t*, size_t size, size_t align);
 
/**
  * realloc function typedef
  */
typedef void* (*mca_allocator_realloc_fn_t)(struct mca_allocator_t*, void*, size_t);

/**
  * free function typedef
  */
typedef void(*mca_allocator_free_fn_t)(struct mca_allocator_t*, void *);


typedef int (*mca_allocator_finalize_fn_t)(
    struct mca_allocator_t* allocator 
);


struct mca_allocator_t {
    mca_allocator_alloc_fn_t alc_alloc;
    mca_allocator_realloc_fn_t alc_realloc;
    mca_allocator_free_fn_t alc_free;
    mca_allocator_finalize_fn_t alc_finalize;
};
typedef struct mca_allocator_t mca_allocator_t;


/**
  *
  */

typedef void* (*mca_allocator_segment_alloc_fn_t)(size_t* size);

/**
  *
  */
typedef void* (*mca_allocator_segment_free_fn_t)(void* segment);


/**
  * module initialization function
  */

typedef struct mca_allocator_t* (*mca_allocator_base_module_init_fn_t)(
    bool *allow_multi_user_threads,
    mca_allocator_segment_alloc_fn_t segment_alloc,
    mca_allocator_segment_free_fn_t segment_free
);

struct mca_allocator_base_module_1_0_0_t {
    mca_base_module_t allocator_version;
    mca_base_module_data_1_0_0_t allocator_data;
    mca_allocator_base_module_init_fn_t allocator_init;
};
typedef struct mca_allocator_base_module_1_0_0_t mca_allocator_base_module_t;

/*
 * Macro for use in modules that are of type ptl v1.0.0
 */
#define MCA_ALLOCATOR_BASE_VERSION_1_0_0 \
  /* mpool v1.0 is chained to MCA v1.0 */ \
  MCA_BASE_VERSION_1_0_0, \
  /* ptl v1.0 */ \
  "allocator", 1, 0, 0

extern int mca_allocator_base_output;

#endif /* MCA_ALLOCATOR_H */

