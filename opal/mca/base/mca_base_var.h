/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2008-2011 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2012-2013 Los Alamos National Security, LLC. All rights
 *                         reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

/** @file 
 * This file presents the MCA variable interface.
 *
 * Note that there are two scopes for MCA variables: "normal" and
 * attributes.  Specifically, all MCA variables are "normal" -- some
 * are special and may also be found on attributes on communicators,
 * datatypes, or windows.
 *
 * In general, these functions are intended to be used as follows:
 *
 * - Creating MCA variables
 * -# Register a variable, get an index back
 * - Using MCA variables
 * -# Lookup a "normal" variable value on a specific index, or
 * -# Lookup an attribute variable on a specific index and
 *    communicator / datatype / window.
 *
 * MCA variables can be defined in multiple different places.  As
 * such, variables are \em resolved to find their value.  The order
 * of resolution is as follows:
 *
 * - An "override" location that is only available to be set via the
 *   mca_base_param API.
 * - Look for an environment variable corresponding to the MCA
 *   variable.
 * - See if a file contains the MCA variable (MCA variable files are
 *   read only once -- when the first time any mca_param_t function is
 *   invoked).
 * - If nothing else was found, use the variable's default value.
 *
 * Note that there is a second header file (mca_base_vari.h)
 * that contains several internal type delcarations for the variable
 * system.  The internal file is only used within the variable system
 * itself; it should not be required by any other Open MPI entities.
 */

#ifndef OPAL_MCA_BASE_VAR_H
#define OPAL_MCA_BASE_VAR_H

#include "opal_config.h"

#include "opal/class/opal_list.h"
#include "opal/class/opal_value_array.h"
#include "opal/mca/base/mca_base_var_enum.h"
#include "opal/mca/base/mca_base_framework.h"
#include "opal/mca/mca.h"

/**
 * The types of MCA variables.
 */
typedef enum {
    /** The variable is of type int. */
    MCA_BASE_VAR_TYPE_INT,
    /** The variable is of type unsigned int */
    MCA_BASE_VAR_TYPE_UNSIGNED_INT,
    /** The variable is of type unsigned long long */
    MCA_BASE_VAR_TYPE_UNSIGNED_LONG_LONG,
    /** The variable is of type size_t */
    MCA_BASE_VAR_TYPE_SIZE_T,
    /** The variable is of type string. */
    MCA_BASE_VAR_TYPE_STRING,
    /** The variable is of type bool */
    MCA_BASE_VAR_TYPE_BOOL,
    /** Maximum variable type. */
    MCA_BASE_VAR_TYPE_MAX
} mca_base_var_type_t;


/**
 * Source of an MCA variable's value
 */
typedef enum {
    /** The default value */
    MCA_BASE_VAR_SOURCE_DEFAULT,
    /** The value came from the command line */
    MCA_BASE_VAR_SOURCE_COMMAND_LINE,
    /** The value came from the environment */
    MCA_BASE_VAR_SOURCE_ENV,
    /** The value came from a file */
    MCA_BASE_VAR_SOURCE_FILE,
    /** The value came a "set" API call */
    MCA_BASE_VAR_SOURCE_SET,
    /** The value came from the override file */
    MCA_BASE_VAR_SOURCE_OVERRIDE,

    /** Maximum source type */
    MCA_BASE_VAR_SOURCE_MAX
} mca_base_var_source_t;

/**
 * MCA variable scopes
 *
 * Equivalent to MPIT scopes
 */
typedef enum {
    /** The value of this variable will not change after it 
        is registered. Implies !MCA_BASE_VAR_FLAG_SETTABLE */
    MCA_BASE_VAR_SCOPE_CONSTANT,
    /** The value of this variable may change but it should not be
        changed using the mca_base_var_set function. Implies
        !MCA_BASE_VAR_FLAG_SETTABLE */
    MCA_BASE_VAR_SCOPE_READONLY,
    /** The value of this variable may be changed locally. */
    MCA_BASE_VAR_SCOPE_LOCAL,
    /** The value of this variable must be set to a consistent value 
        within a group */
    MCA_BASE_VAR_SCOPE_GROUP,
    /** The value of this variable must be set to the same value 
        within a group */
    MCA_BASE_VAR_SCOPE_GROUP_EQ,
    /** The value of this variable must be set to a consistent value 
        for all processes */
    MCA_BASE_VAR_SCOPE_ALL,
    /** The value of this variable must be set to the same value 
        for all processes */
    MCA_BASE_VAR_SCOPE_ALL_EQ,
    MCA_BASE_VAR_SCOPE_MAX
} mca_base_var_scope_t;

typedef enum {
    OPAL_INFO_LVL_1,
    OPAL_INFO_LVL_2,
    OPAL_INFO_LVL_3,
    OPAL_INFO_LVL_4,
    OPAL_INFO_LVL_5,
    OPAL_INFO_LVL_6,
    OPAL_INFO_LVL_7,
    OPAL_INFO_LVL_8,
    OPAL_INFO_LVL_9,
    OPAL_INFO_LVL_MAX
} mca_base_var_info_lvl_t;

typedef enum {
    MCA_BASE_VAR_SYN_FLAG_DEPRECATED = 0x0001,
    MCA_BASE_VAR_SYN_FLAG_INTERNAL   = 0x0002
} mca_base_var_syn_flag_t;

typedef enum {
    /** Variable is internal (hidden from *_info/MPIT) */
    MCA_BASE_VAR_FLAG_INTERNAL     = 0x0001,
    /** Variable will always be the default value. Implies
        !MCA_BASE_VAR_FLAG_SETTABLE */
    MCA_BASE_VAR_FLAG_DEFAULT_ONLY = 0x0002,
    /** Variable can be set with mca_base_var_set() */
    MCA_BASE_VAR_FLAG_SETTABLE     = 0x0004,
    /** Variable is deprecated */
    MCA_BASE_VAR_FLAG_DEPRECATED   = 0x0008,
    /** Variable has been overridden */
    MCA_BASE_VAR_FLAG_OVERRIDE     = 0x0010,
    /** Variable may not be set from a file */
    MCA_BASE_VAR_FLAG_ENVIRONMENT_ONLY = 0x0020,
    /** Variable should be deregistered when the group
        is deregistered */
    MCA_BASE_VAR_FLAG_DWG          = 0x0040
} mca_base_var_flag_t;


/**
 * Types for MCA parameters.
 */
typedef union {
    /** Integer value */
    int intval;
    /** Unsigned int value */
    unsigned int uintval;
    /** String value */
    char *stringval;
    /** Boolean value */
    bool boolval;
    /** unsigned long long value */
    unsigned long long ullval;
    /** size_t value */
    size_t sizetval;
} mca_base_var_storage_t;


/**
 * Entry for holding information about an MCA variable.
 */
struct mca_base_var_t {
    /** Allow this to be an OPAL OBJ */
    opal_object_t super;

    /** Variable index. This will remain constant until mca_base_var_finalize()
        is called. */
    int mbv_index;
    /** Group index. This will remain constant until mca_base_var_finalize()
        is called. This variable will be deregistered if the associated group
        is deregistered with mca_base_var_group_deregister() */
    int mbv_group_index;

    /** Info level of this variable */
    mca_base_var_info_lvl_t mbv_info_lvl;

    /** Enum indicating the type of the variable (integer, string, boolean) */
     mca_base_var_type_t mbv_type;

    /** String of the variable name */
    char *mbv_variable_name;
    /** Full variable name, in case it is not <framework>_<component>_<param> */
    char *mbv_full_name;
    /** Long variable name <project>_<framework>_<component>_<name> */
    char *mbv_long_name;

    /** List of synonym names for this variable.  This *must* be a
        pointer (vs. a plain opal_list_t) because we copy this whole
        struct into a new var for permanent storage
        (opal_vale_array_append_item()), and the internal pointers in
        the opal_list_t will be invalid when that happens.  Hence, we
        simply keep a pointer to an external opal_list_t.  Synonyms
        are uncommon enough that this is not a big performance hit. */
    opal_value_array_t mbv_synonyms;

    /** Variable flags */
    mca_base_var_flag_t  mbv_flags;

    /** Variable scope */
    mca_base_var_scope_t  mbv_scope;

    /** Source of the current value */
    mca_base_var_source_t mbv_source;

    /** Synonym for */
    int mbv_synonym_for;

    /** Variable description */
    char *mbv_description;

    /** File the value came from */
    char *mbv_source_file;

    /** Value enumerator (only valid for integer variables) */
    mca_base_var_enum_t *mbv_enumerator;

    /** Bind value for this variable (0 - none) */
    int mbv_bind;

    /** Storage for this variable */
    mca_base_var_storage_t *mbv_storage;
};
/**
 * Convenience typedef.
 */
typedef struct mca_base_var_t mca_base_var_t;

struct mca_base_var_group_t {
    opal_list_item_t super;

    /** Index of group */
    int group_index;

    /** Group is valid (registered) */
    bool group_isvalid;

    /** Group name */
    char *group_full_name;

    char *group_project;
    char *group_framework;
    char *group_component;

    /** Group help message (description) */
    char *group_description;

    /** Integer value array of subgroup indices */
    opal_value_array_t group_subgroups;

    /** Integer array of group variables */
    opal_value_array_t group_vars;
};

typedef struct mca_base_var_group_t mca_base_var_group_t;

/*
 * Global functions for MCA
 */

BEGIN_C_DECLS

/**
 * Object declarayion for mca_base_var_t
 */
OPAL_DECLSPEC OBJ_CLASS_DECLARATION(mca_base_var_t);


/**
 * Object declaration for mca_base_var_group_t
 */
OPAL_DECLSPEC OBJ_CLASS_DECLARATION(mca_base_var_group_t);

/**
 * Initialize the MCA variable system.
 *
 * @retval OPAL_SUCCESS
 *
 * This function initalizes the MCA variable system.  It is
 * invoked internally (by mca_base_open()) and is only documented
 * here for completeness.
 */
OPAL_DECLSPEC int mca_base_var_init(void);

/**
 * Register an MCA variable group
 *
 * @param[in] project_name Project name for this group.
 * @param[in] framework_name Framework name for this group.
 * @param[in] component_name Component name for this group.
 * @param[in] descrition Description of this group.
 *
 * @retval index Unique group index
 * @return opal error code on Error
 *
 * Create an MCA variable group. If the group already exists
 * this call is equivalent to mca_base_ver_find_group().
 */
OPAL_DECLSPEC int mca_base_var_group_register(const char *project_name,
                                              const char *framework_name,
                                              const char *component_name,
                                              const char *description);

/**
 * Register an MCA variable group for a component
 *
 * @param[in] component [in] Pointer to the component for which the
 * group is being registered.
 * @param[in] description Description of this group.
 *
 * @retval index Unique group index
 * @return opal error code on Error
 */
OPAL_DECLSPEC int mca_base_var_group_component_register (const mca_base_component_t *component,
                                                         const char *description);

/**
 * Deregister an MCA param group
 *
 * @param group_index [in] Group index from mca_base_var_group_register (),
 * mca_base_var_group_find().
 *
 * This call deregisters all associated variables and subgroups.
 */
OPAL_DECLSPEC int mca_base_var_group_deregister (int group_index);

/**
 * Find an MCA group
 *
 * @param project_name [in] Project name
 * @param framework_name [in] Type name
 * @param component_name [in] Component name
 */
OPAL_DECLSPEC int mca_base_var_group_find (const char *project_name,
                                           const char *framework_name,
                                           const char *component_name);

/**
 * Dump info from a group
 *
 * @param[in] group_index Group index
 * @param[out] group Storage for the group object pointer.
 *
 * @retval OPAL_ERR_NOT_FOUND If the group specified by group_index does not exist.
 * @retval OPAL_ERR_OUT_OF_RESOURCE If memory allocation fails.
 * @retval OPAL_SUCCESS If the group is dumped successfully.
 *
 * The returned pointer belongs to the MCA variable system. Do not modify/release/retain
 * the pointer.
 */
OPAL_DECLSPEC int mca_base_var_group_get (const int group_index,
                                          const mca_base_var_group_t **group);

/**
 * Set/unset a flags for all variables in a group.
 *
 * @param[in] group_index Index of group
 * @param[in] flag Flag(s) to set or unset.
 * @param[in] set Boolean indicating whether to set flag(s).
 *
 * Set a flag for every variable in a group. See mca_base_var_set_flag() for more info.
 */
OPAL_DECLSPEC int mca_base_var_group_set_var_flag (const int group_index,
                                                   mca_base_var_flag_t flags,
                                                   bool set);

/**
 * Get the number of registered MCA groups
 *
 * @retval count Number of registered MCA groups
 */
OPAL_DECLSPEC int mca_base_var_group_get_count (void);

/**
 * Get a relative timestamp for the MCA group system
 *
 * @retval stamp 
 *
 * This value will change if groups or variables are either added or removed.
 */
OPAL_DECLSPEC int mca_base_var_group_get_stamp (void);

/**
 * Register an MCA variable
 *
 * @param[in] project_name The name of the project associated with
 * this variable
 * @param[in] framework_name The name of the framework associated with
 * this variable
 * @param[in] component_name The name of the component associated with
 * this variable
 * @param[in] variable_name The name of this variable
 * @param[in] description A string describing the use and valid
 * values of the variable (string).
 * @param[in] type The type of this variable (string, int, bool).
 * @param[in] enumerator Enumerator describing valid values.
 * @param[in] bind Hint for MPIT to specify type of binding (0 = none)
 * @param[in] flags Flags for this variable.
 * @param[in] info_lvl Info level of this variable
 * @param[in] scope Indicates the scope of this variable
 * @param[in,out] storage Pointer to the value's location.
 *
 * @retval index Index value representing this variable.
 * @retval OPAL_ERR_OUT_OF_RESOURCE Upon failure to allocate memory.
 * @retval OPAL_ERROR Upon failure to register the variable.
 *
 * This function registers an MCA variable and associates it
 * with a specific group.
 *
 * The {description} is a string of arbitrary length (verbose is
 * good!) for explaining what the variable is for and what its
 * valid values are.  This message is used in help messages, such
 * as the output from the ompi_info executable.
 *
 * The {enumerator} describes the valid values of an integer
 * variable. The variable may be set to either the enumerator value
 * (0, 1, 2, etc) or a string representing that value. The
 * value provided by the user will be compared against the
 * values in the enumerator. The {enumerator} is not valid with
 * any other type of variable. {enumerator} is retained until
 * either the variable is deregistered using mca_base_var_deregister(),
 * mca_base_var_group_deregister(), or mca_base_var_finalize().
 *
 * The {flags} indicate attributes of this variable (internal,
 * settable, default only, etc).
 *
 * If MCA_BASE_VAR_FLAG_SETTABLE is set in {flags}, this variable
 * may be set using mca_base_var_set_value().
 *
 * If MCA_BASE_VAR_FLAG_INTERNAL is set in {flags}, this variable
 * is not shown by default in the output of ompi_info.  That is,
 * this variable is considered internal to the Open MPI implementation
 * and is not supposed to be viewed / changed by the user.
 *
 * If MCA_BASE_VAR_FLAG_DEFAULT_ONLY is set in {flags}, then the
 * value provided in storage will not be modified by the MCA
 * variable system. It is up to the caller to specify (using the scope)
 * if this value may change (MCA_BASE_VAR_SCOPE_READONLY)
 * or remain constant (MCA_BASE_VAR_SCOPE_CONSTANT).
 * MCA_BASE_VAR_FLAG_DEFAULT_ONLY must not be specified with
 * MCA_BASE_VAR_FLAG_SETTABLE.
 *
 * Set MCA_BASE_VAR_FLAG_DEPRECATED in {flags} to indicate that
 * this variable name is deprecated. The user will get a warning
 * if they set this variable.
 *
 * The {scope} is for informational purposes to indicate how this
 * variable should be set or if it is considered constant or readonly.
 * A readonly scope means something different than setting {read_only}
 * to true. A readonly scope will still allow the variable to be
 * overridden by a file or environment variable.
 *
 * The {storage} pointer points to a char *, int, or bool where the
 * value of this variable is stored. The {type} indicates the type
 * of this pointer. The initial value passed to this function may
 * be overwritten if the MCA_BASE_VAR_FLAG_DEFAULT_ONLY flag is not
 * set and either an environment variable or a file value for this
 * variable is set. If {storage} points to a char * the value will
 * be duplicated and it is up to the caller to retain and free the
 * original value if needed. Any string value set when this
 * variable is deregistered (including finalize) will be freed
 * automatically.
 */
OPAL_DECLSPEC int mca_base_var_register (const char *project_name, const char *framework_name,
                                         const char *component_name, const char *variable_name,
                                         const char *description, mca_base_var_type_t type,
                                         mca_base_var_enum_t *enumerator, int bind, mca_base_var_flag_t flags,
                                         mca_base_var_info_lvl_t info_lvl,
                                         mca_base_var_scope_t scope, void *storage);

/**
 * Convinience function for registering a variable associated with a component.
 * See mca_base_var_register().
 */
OPAL_DECLSPEC int mca_base_component_var_register (const mca_base_component_t *component,
                                                   const char *variable_name, const char *description,
                                                   mca_base_var_type_t type, mca_base_var_enum_t *enumerator,
                                                   int bind, mca_base_var_flag_t flags,
                                                   mca_base_var_info_lvl_t info_lvl,
                                                   mca_base_var_scope_t scope, void *storage);

/**
 * Convinience function for registering a variable associated with a framework. This
 * function is equivalent to mca_base_var_register with component_name = "base" and
 * with the MCA_BASE_VAR_FLAG_DWG set. See mca_base_var_register().
 */
OPAL_DECLSPEC int mca_base_framework_var_register (const mca_base_framework_t *framework,
                                     const char *variable_name,
                                     const char *help_msg, mca_base_var_type_t type,
                                     mca_base_var_enum_t *enumerator, int bind,
                                     mca_base_var_flag_t flags,
                                     mca_base_var_info_lvl_t info_level,
                                     mca_base_var_scope_t scope, void *storage);


/**
 * Register a synonym name for an MCA variable.
 *
 * @param[in] synonym_for The index of the original variable. This index
 * must not refer to a synonym.
 * @param[in] project_name The project this synonym belongs to. Should
 * not be NULL (except for legacy reasons).
 * @param[in] framework_name The framework this synonym belongs to.
 * @param[in] component_name The component this synonym belongs to.
 * @param[in] synonym_name The synonym name.
 * @param[in] flags Flags for this synonym.
 *
 * @returns index Variable index for new synonym on success.
 * @returns OPAL_ERR_BAD_VAR If synonym_for does not reference a valid
 * variable.
 * @returns OPAL_ERR_OUT_OF_RESOURCE If memory could not be allocated.
 * @returns OPAL_ERROR For all other errors.
 * 
 * Upon success, this function creates a synonym MCA variable
 * that will be treated almost exactly like the original.  The
 * type (int or string) is irrelevant; this function simply
 * creates a new name that by which the same variable value is
 * accessible.  
 *
 * Note that the original variable name has precendence over all
 * synonyms.  For example, consider the case if variable is
 * originally registered under the name "A" and is later
 * registered with synonyms "B" and "C".  If the user sets values
 * for both MCA variable names "A" and "B", the value associated
 * with the "A" name will be used and the value associated with
 * the "B" will be ignored (and will not even be visible by the
 * mca_base_var_*() API).  If the user sets values for both MCA
 * variable names "B" and "C" (and does *not* set a value for
 * "A"), it is undefined as to which value will be used.
 */
OPAL_DECLSPEC int mca_base_var_register_synonym (int synonym_for, const char *project_name,
                                                 const char *framework_name,
                                                 const char *component_name,
                                                 const char *synonym_name,
                                                 mca_base_var_syn_flag_t flags);

/**
 * Deregister a MCA variable or synonym
 *
 * @param index Index returned from mca_base_var_register() or
 * mca_base_var_register_synonym().
 *
 * Deregistering a variable does not free the index or any memory assoicated
 * with the variable. All memory will be freed and the index released when
 * mca_base_var_finalize() is called.
 *
 * If an enumerator is associated with this variable it will be dereferenced.
 */
OPAL_DECLSPEC int mca_base_var_deregister(int index);


/**
 * Get the current value of an MCA variable.
 *
 * @param[in] index Index of variable
 * @param[in,out] value Pointer to copy the value to. Can be NULL.
 * @param[in,out] value_size Size of memory pointed to by value.
 * copied size will be returned in value_size.
 * @param[out] source Source of current value. Can be NULL.
 * @param[out] source_file Source file for the current value if
 * it was set from a file.
 *
 * @return OPAL_ERROR Upon failure.  The contents of value are
 * undefined.
 * @return OPAL_SUCCESS Upon success. value (if not NULL) will be filled
 * with the variable's current value. value_size will contain the size
 * copied. source (if not NULL) will contain the source of the variable.
 *
 * Note: The value can be changed by the registering code without using
 * the mca_base_var_* interface so the source may be incorrect.
 */
OPAL_DECLSPEC int mca_base_var_get_value (int index, const void *value,
                                          mca_base_var_source_t *source,
                                          const char **source_file);

/**
 * Sets an "override" value for an integer MCA variable.
 *
 * @param[in] index Index of MCA variable to set
 * @param[in] value Pointer to the value to set. Should point to
 * a char * for string variables or a int * for integer variables.
 * @param[in] size Size of value.
 * @param[in] source Source of this value.
 * @param[in] source_file Source file if source is MCA_BASE_VAR_SOURCE_FILE.
 *
 * @retval OPAL_SUCCESS  Upon success.
 * @retval OPAL_ERR_PERM If the variable is not settable.
 * @retval OPAL_ERR_BAD_PARAM If the variable does not exist or has
 * been deregistered.
 * @retval OPAL_ERROR On other error.
 *
 * This function sets the value of an MCA variable. This value will
 * overwrite the current value of the variable (or if index represents
 * a synonym the variable the synonym represents) if the value is
 * settable.
 */
OPAL_DECLSPEC int mca_base_var_set_value (int index, void *value, size_t size,
                                          mca_base_var_source_t source,
                                          const char *source_file);

/**
 * Get the string name corresponding to the MCA variable
 * value in the environment.
 *
 * @param param_name Name of the type containing the variable.
 *
 * @retval string A string suitable for setenv() or appending to
 * an environ-style string array.
 * @retval NULL Upon failure.
 *
 * The string that is returned is owned by the caller; if
 * appropriate, it must be eventually freed by the caller.
 */
OPAL_DECLSPEC int mca_base_var_env_name(const char *param_name,
                                        char **env_name);

/**
 * Find the index for an MCA variable based on its names.
 *
 * @param type Name of the type containing the variable.
 * @param component Name of the component containing the variable.
 * @param param Name of the variable.
 *
 * @retval OPAL_ERROR If the variable was not found.
 * @retval index If the variable was found.
 *
 * It is not always convenient to widely propagate a variable's index
 * value, or it may be necessary to look up the variable from a
 * different component -- where it is not possible to have the return
 * value from mca_base_var_reg_int() or mca_base_var_reg_string().
 * This function can be used to look up the index of any registered
 * variable.  The returned index can be used with
 * mca_base_var_lookup_int() and mca_base_var_lookup_string().
 */
OPAL_DECLSPEC int mca_base_var_find (const char *project_name,
                                     const char *type_name,
                                     const char *component_name,
                                     const char *param_name);

/**
 * Check that two MCA variables were not both set to non-default
 * values.
 *
 * @param type_a [in] Framework name of variable A (string).
 * @param component_a [in] Component name of variable A (string).
 * @param param_a [in] Variable name of variable A (string.
 * @param type_b [in] Framework name of variable A (string).
 * @param component_b [in] Component name of variable A (string).
 * @param param_b [in] Variable name of variable A (string.
 *
 * This function is useful for checking that the user did not set both
 * of 2 mutually-exclusive MCA variables.
 *
 * This function will print an opal_show_help() message and return
 * OPAL_ERR_BAD_VAR if it finds that the two variables both have
 * value sources that are not MCA_BASE_VAR_SOURCE_DEFAULT.  This
 * means that both variables have been set by the user (i.e., they're
 * not default values).
 *
 * Note that opal_show_help() allows itself to be hooked, so if this
 * happens after the aggregated orte_show_help() system is
 * initialized, the messages will be aggregated (w00t).
 *
 * @returns OPAL_ERR_BAD_VAR if the two variables have sources that
 * are not MCA_BASE_VAR_SOURCE_DEFAULT.
 * @returns OPAL_SUCCESS otherwise.
 */
OPAL_DECLSPEC int mca_base_var_check_exclusive (const char *project,
                                                const char *type_a,
                                                const char *component_a,
                                                const char *param_a,
                                                const char *type_b,
                                                const char *component_b,
                                                const char *param_b);

/**
 * Set or unset a flag on a variable.
 *
 * @param[in] index Index of variable
 * @param[in] flag Flag(s) to set or unset.
 * @param[in] set Boolean indicating whether to set flag(s).
 *
 * @returns OPAL_SUCCESS If the flags are set successfully.
 * @returns OPAL_ERR_BAD_PARAM If the variable is not registered.
 * @returns OPAL_ERROR Otherwise
 */
OPAL_DECLSPEC int mca_base_var_set_flag(int index, mca_base_var_flag_t flag,
                                        bool set);

/**
 * Obtain basic info on a single variable (name, help message, etc)
 *
 * @param[in] index Valid variable index.
 * @param[out] var Storage for the variable pointer.
 *
 * @retval OPAL_SUCCESS Upon success.
 * @retval opal error code Upon failure.
 *
 * The returned pointer belongs to the MCA variable system. Do not
 * modify/free/retain the pointer.
 */
OPAL_DECLSPEC int mca_base_var_get (int index, const mca_base_var_t **var);

/**
 * Obtain the number of variables that have been registered.
 *
 * @retval count on success
 * @return opal error code on error
 *
 * Note: This function does not return the number of valid MCA variables as
 * mca_base_var_deregister() has no impact on the variable count. The count
 * returned is equal to the number of calls to mca_base_var_register with
 * unique names. ie. two calls with the same name will not affect the count.
 */
OPAL_DECLSPEC int mca_base_var_get_count (void);

/**
 * Obtain a list of enironment variables describing the all
 * valid (non-default) MCA variables and their sources.
 *
 * @param[out] env A pointer to an argv-style array of key=value
 * strings, suitable for use in an environment
 * @param[out] num_env A pointer to an int, containing the length
 * of the env array (not including the final NULL entry).
 * @param[in] internal Whether to include internal variables.
 *
 * @retval OPAL_SUCCESS Upon success.
 * @retval OPAL_ERROR Upon failure.
 *
 * This function is similar to mca_base_var_dump() except that
 * its output is in terms of an argv-style array of key=value
 * strings, suitable for using in an environment.
 */
OPAL_DECLSPEC int mca_base_var_build_env(char ***env, int *num_env,
                                         bool internal);

/**
 * Shut down the MCA variable system (normally only invoked by the
 * MCA framework itself).
 *
 * @returns OPAL_SUCCESS This function never fails.
 *
 * This function shuts down the MCA variable repository and frees all
 * associated memory.  No other mca_base_var*() functions can be
 * invoked after this function.
 *
 * This function is normally only invoked by the MCA framework itself
 * when the process is shutting down (e.g., during MPI_FINALIZE).  It
 * is only documented here for completeness.
 */
OPAL_DECLSPEC int mca_base_var_finalize(void);

typedef enum {
    MCA_BASE_VAR_DUMP_READABLE = 0,
    MCA_BASE_VAR_DUMP_PARSABLE = 1,
    MCA_BASE_VAR_DUMP_SIMPLE   = 2,
} mca_base_var_dump_type_t;

/**
 * Dump strings for variable at index.
 *
 * @param[in]  index Variable index
 * @param[out] out   Array of strings representing this variable
 * @param[in]  flags Flags indication how to output variable
 *
 * This functions returns an array strings for the variable. All strings and the array
 * need to be freed by the caller.
 */
OPAL_DECLSPEC int mca_base_var_dump(int index, char ***out, mca_base_var_dump_type_t output_type);

END_C_DECLS

#endif /* OPAL_MCA_BASE_VAR_H */
