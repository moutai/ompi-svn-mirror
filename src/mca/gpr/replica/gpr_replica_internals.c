/*
 * $HEADER$
 */
/** @file:
 *
 * The Open MPI general purpose registry - support functions.
 *
 */

/*
 * includes
 */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#include "ompi_config.h"
#include "include/constants.h"
#include "util/output.h"
#include "mca/mca.h"
#include "mca/oob/base/base.h"
#include "mca/gpr/base/base.h"
#include "gpr_replica.h"
#include "gpr_replica_internals.h"

/*
 *
 */
mca_gpr_registry_segment_t *gpr_replica_find_seg(char *segment)
{
    mca_gpr_keytable_t *ptr_seg;
    mca_gpr_registry_segment_t *seg;

    /* search the registry segments to find which one is being referenced */
    for (ptr_seg = (mca_gpr_keytable_t*)ompi_list_get_first(&mca_gpr_replica_head.segment_dict);
	 ptr_seg != (mca_gpr_keytable_t*)ompi_list_get_end(&mca_gpr_replica_head.segment_dict);
	 ptr_seg = (mca_gpr_keytable_t*)ompi_list_get_next(ptr_seg)) {
	if (0 == strcmp(segment, ptr_seg->token)) {
	    /* search mca_gpr_replica_head to find segment */
	    for (seg=(mca_gpr_registry_segment_t*)ompi_list_get_first(&mca_gpr_replica_head.registry);
		 seg != (mca_gpr_registry_segment_t*)ompi_list_get_end(&mca_gpr_replica_head.registry);
		 seg = (mca_gpr_registry_segment_t*)ompi_list_get_next(seg)) {
		if(seg->segment == ptr_seg->key) {
		    return(seg);
		}
	    }
	}
    }
    return(NULL); /* couldn't find the specified segment */
}

mca_gpr_keytable_t *gpr_replica_find_dict_entry(char *segment, char *token)
{
    mca_gpr_keytable_t *ptr_seg;
    mca_gpr_keytable_t *ptr_key;
    mca_gpr_registry_segment_t *seg;

    /* search the registry segments to find which one is being referenced */
    for (ptr_seg = (mca_gpr_keytable_t*)ompi_list_get_first(&mca_gpr_replica_head.segment_dict);
	 ptr_seg != (mca_gpr_keytable_t*)ompi_list_get_end(&mca_gpr_replica_head.segment_dict);
	 ptr_seg = (mca_gpr_keytable_t*)ompi_list_get_next(ptr_seg)) {
	if (0 == strcmp(segment, ptr_seg->token)) {
	    if (NULL == token) { /* just want segment token-key pair */
		return(ptr_seg);
	    }
	    /* search registry to find segment */
	    for (seg=(mca_gpr_registry_segment_t*)ompi_list_get_first(&mca_gpr_replica_head.registry);
		 seg != (mca_gpr_registry_segment_t*)ompi_list_get_end(&mca_gpr_replica_head.registry);
		 seg = (mca_gpr_registry_segment_t*)ompi_list_get_next(seg)) {
		if(seg->segment == ptr_seg->key) {
		    /* got segment - now find specified token-key pair in that dictionary */
		    for (ptr_key = (mca_gpr_keytable_t*)ompi_list_get_first(&seg->keytable);
			 ptr_key != (mca_gpr_keytable_t*)ompi_list_get_end(&seg->keytable);
			 ptr_key = (mca_gpr_keytable_t*)ompi_list_get_next(ptr_key)) {
			if (0 == strcmp(token, ptr_key->token)) {
			    return(ptr_key);
			}
		    }
		    return(NULL); /* couldn't find the specified entry */
		}
	    }
	    return(NULL); /* couldn't find segment, even though we found entry in registry dict */
	}
    }
    return(NULL); /* couldn't find segment token-key pair */
}


mca_gpr_replica_key_t gpr_replica_get_key(char *segment, char *token)
{
    mca_gpr_keytable_t *ptr_key;

    /* find registry segment */
    ptr_key = gpr_replica_find_dict_entry(segment, NULL);
    if (NULL != ptr_key) {
	if (NULL == token) { /* only want segment key */
	    return(ptr_key->key);
	}
	/* if token specified, find the dictionary entry that matches token */
	ptr_key = gpr_replica_find_dict_entry(segment, token);
	if (NULL != ptr_key) {
	    return(ptr_key->key);
	}
	return MCA_GPR_REPLICA_KEY_MAX; /* couldn't find dictionary entry */
    }
    return MCA_GPR_REPLICA_KEY_MAX; /* couldn't find segment */
}


ompi_list_t *gpr_replica_get_key_list(char *segment, char **tokens)
{
    ompi_list_t *keys;
    char **token;
    mca_gpr_keytable_t *keyptr;

    token = tokens;
    keys = OBJ_NEW(ompi_list_t);

    /* protect against errors */
    if (NULL == segment || NULL == tokens || NULL == *token) {
	return keys;
    }

    while (NULL != *token) {  /* traverse array of tokens until NULL */
	keyptr = OBJ_NEW(mca_gpr_keytable_t);
	keyptr->token = strdup(*token);
	keyptr->key = gpr_replica_get_key(segment, *token);
	ompi_list_append(keys, &keyptr->item);
	token++;
    }
    return keys;
}

int gpr_replica_define_key(char *segment, char *token)
{
    mca_gpr_registry_segment_t *seg;
    mca_gpr_keytable_t *ptr_seg, *ptr_key, *new;

    /* protect against errors */
    if (NULL == segment) {
	return OMPI_ERROR;
    }

    /* if token is NULL, then this is defining a segment name. Check dictionary to ensure uniqueness */
    if (NULL == token) {
	for (ptr_seg = (mca_gpr_keytable_t*)ompi_list_get_first(&mca_gpr_replica_head.segment_dict);
	     ptr_seg != (mca_gpr_keytable_t*)ompi_list_get_end(&mca_gpr_replica_head.segment_dict);
	     ptr_seg = (mca_gpr_keytable_t*)ompi_list_get_next(ptr_seg)) {
	    if (0 == strcmp(segment, ptr_seg->token)) {
		return OMPI_EXISTS;
	    }
	}

	/* okay, name is not previously taken. Define a key value for it and return */
	new = OBJ_NEW(mca_gpr_keytable_t);
	new->token = strdup(segment);
	if (0 == ompi_list_get_size(&mca_gpr_replica_head.freekeys)) { /* no keys waiting for reuse */
	    if (MCA_GPR_REPLICA_KEY_MAX-2 > mca_gpr_replica_head.lastkey) {  /* have a key left */
	    mca_gpr_replica_head.lastkey++;
	    new->key = mca_gpr_replica_head.lastkey;
	    } else {  /* out of keys */
		return OMPI_ERR_OUT_OF_RESOURCE;
	    }
	} else {
	    ptr_key = (mca_gpr_keytable_t*)ompi_list_remove_first(&mca_gpr_replica_head.freekeys);
	    new->key = ptr_key->key;
	}
	ompi_list_append(&mca_gpr_replica_head.segment_dict, &new->item);
	return OMPI_SUCCESS;
    }

    /* okay, token is specified */
    /* search the registry segments to find which one is being referenced */
    seg = gpr_replica_find_seg(segment);
    if (NULL != seg) {
	/* using that segment, check dictionary to ensure uniqueness */
	for (ptr_key = (mca_gpr_keytable_t*)ompi_list_get_first(&seg->keytable);
	     ptr_key != (mca_gpr_keytable_t*)ompi_list_get_end(&seg->keytable);
	     ptr_key = (mca_gpr_keytable_t*)ompi_list_get_next(ptr_key)) {
	    if (0 == strcmp(token, ptr_key->token)) {
		return OMPI_EXISTS; /* already taken, report error */
	    }
	}
	/* okay, token is unique - create dictionary entry */
	new = OBJ_NEW(mca_gpr_keytable_t);
	new->token = strdup(token);
	if (0 == ompi_list_get_size(&seg->freekeys)) { /* no keys waiting for reuse */
	    seg->lastkey++;
	    new->key = seg->lastkey;
	} else {
	    ptr_key = (mca_gpr_keytable_t*)ompi_list_remove_first(&seg->freekeys);
	    new->key = ptr_key->key;
	}
	ompi_list_append(&seg->keytable, &new->item);
	return OMPI_SUCCESS;
    }
    /* couldn't find segment */
    return OMPI_ERROR;
}

int gpr_replica_delete_key(char *segment, char *token)
{
    mca_gpr_registry_segment_t *seg;
    mca_gpr_registry_core_t *reg, *prev;
    mca_gpr_keytable_t *ptr_seg, *ptr_key, *new, *regkey;

    /* protect ourselves against errors */
    if (NULL == segment) {
	return(OMPI_ERROR);
    }

    /* find the segment */
    seg = gpr_replica_find_seg(segment);
    if (NULL != seg) {

	/* if specified token is NULL, then this is deleting a segment name.*/
	if (NULL == token) {
	    if (OMPI_SUCCESS != gpr_replica_empty_segment(seg)) { /* couldn't empty segment */
		return OMPI_ERROR;
	    }
	    /* now remove the dictionary entry from the global registry dictionary*/
	    ptr_seg = gpr_replica_find_dict_entry(segment, NULL);
	    if (NULL == ptr_seg) { /* failed to find dictionary entry */
		return OMPI_ERROR;
	    }
	    /* add key to global registry's freekey list */
	    new = OBJ_NEW(mca_gpr_keytable_t);
	    new->token = NULL;
	    new->key = ptr_seg->key;
	    ompi_list_append(&mca_gpr_replica_head.freekeys, &new->item);

	    /* remove the dictionary entry */
	    ompi_list_remove_item(&mca_gpr_replica_head.segment_dict, &ptr_seg->item);
	    return(OMPI_SUCCESS);

	} else {  /* token not null, so need to find dictionary element to delete */
	    ptr_key = gpr_replica_find_dict_entry(segment, token);
	    if (NULL != ptr_key) {
		/* found key in dictionary */
		/* need to search this segment's registry to find all instances of key - then delete them */
		for (reg = (mca_gpr_registry_core_t*)ompi_list_get_first(&seg->registry_entries);
		     reg != (mca_gpr_registry_core_t*)ompi_list_get_end(&seg->registry_entries);
		     reg = (mca_gpr_registry_core_t*)ompi_list_get_next(reg)) {

		    /* check the key list */
		    for (regkey = (mca_gpr_keytable_t*)ompi_list_get_first(&reg->keys);
			 (regkey != (mca_gpr_keytable_t*)ompi_list_get_end(&reg->keys))
			     && (regkey->key != ptr_key->key);
			 regkey = (mca_gpr_keytable_t*)ompi_list_get_next(regkey));
		    if (regkey != (mca_gpr_keytable_t*)ompi_list_get_end(&reg->keys)) {
			ompi_list_remove_item(&reg->keys, &regkey->item);
		    }
		    /* if this was the last key, then remove the registry entry itself */
		    if (0 == ompi_list_get_size(&reg->keys)) {
			while (0 < ompi_list_get_size(&reg->subscriber)) {
			    ompi_list_remove_last(&reg->subscriber);
			}
			prev = (mca_gpr_registry_core_t*)ompi_list_get_prev(reg);
			ompi_list_remove_item(&seg->registry_entries, &reg->item);
			reg = prev;
		    }
		}

		/* add key to this segment's freekey list */
		new = OBJ_NEW(mca_gpr_keytable_t);
		new->token = NULL;
		new->key = ptr_key->key;
		ompi_list_append(&seg->freekeys, &new->item);

		/* now remove the dictionary entry from the segment's dictionary */
		ompi_list_remove_item(&seg->keytable, &ptr_key->item);
		return(OMPI_SUCCESS);
	    }
	    return(OMPI_ERROR); /* if we get here, then we couldn't find token in dictionary */
	}
    }
    return(OMPI_ERROR); /* if we get here, then we couldn't find segment */
}

int gpr_replica_empty_segment(mca_gpr_registry_segment_t *seg)
{
    /* need to free memory from each entry - remove_last returns pointer to the entry */

    /* empty the segment's registry */
    while (0 < ompi_list_get_size(&seg->registry_entries)) {
	ompi_list_remove_last(&seg->registry_entries);
    }

    /* empty the segment's dictionary */
    while (0 < ompi_list_get_size(&seg->keytable)) {
	ompi_list_remove_last(&seg->keytable);
    }
    /* empty the list of free keys */
    while (0 < ompi_list_get_size(&seg->freekeys)) {
	ompi_list_remove_last(&seg->freekeys);
    }
    /* now remove segment from global registry */
    ompi_list_remove_item(&mca_gpr_replica_head.registry, &seg->item);

    return OMPI_SUCCESS;
}

/*
 * A mode of "NONE" or "OVERWRITE" defaults to "XAND" behavior
 */
bool gpr_replica_check_key_list(ompi_registry_mode_t mode, ompi_list_t *key_list, mca_gpr_registry_core_t *entry)
{
    mca_gpr_keylist_t *keyptr;
    mca_gpr_keytable_t *key;
    size_t num_keys_search, num_keys_entry, num_found;
    bool exclusive, no_match;

    if (OMPI_REGISTRY_NONE == mode ||
	OMPI_REGISTRY_OVERWRITE == mode) { /* set default behavior for search */
	mode = OMPI_REGISTRY_XAND;
    }

    num_keys_search = ompi_list_get_size(key_list);
    num_keys_entry = ompi_list_get_size(&entry->keys);

    /* take care of trivial cases that don't require search */
    if ((OMPI_REGISTRY_XAND & mode) &&
	(num_keys_search != num_keys_entry)) { /* can't possibly turn out "true" */
	ompi_output(mca_gpr_base_output, "xand violation");
	return false;
    }

    if ((OMPI_REGISTRY_AND & mode) &&
	(num_keys_search > num_keys_entry)) {  /* can't find enough matches */
	ompi_output(mca_gpr_base_output, "and violation");
	return false;
    }

    /* okay, have to search for remaining possibilities */
    num_found = 0;
    exclusive = true;
    for (keyptr = (mca_gpr_keylist_t*)ompi_list_get_first(&entry->keys);
	 keyptr != (mca_gpr_keylist_t*)ompi_list_get_end(&entry->keys);
	 keyptr = (mca_gpr_keylist_t*)ompi_list_get_next(keyptr)) {
	no_match = true;
	for (key = (mca_gpr_keytable_t*)ompi_list_get_first(key_list);
	     (key != (mca_gpr_keytable_t*)ompi_list_get_end(key_list)) && no_match;
	     key = (mca_gpr_keytable_t*)ompi_list_get_next(key)) {
	    if (key->key == keyptr->key) { /* found a match */
		num_found++;
		no_match = false;
		if (OMPI_REGISTRY_OR & mode) { /* only need one match */
		    return true;
		}
	    }
	}
	if (no_match) {
	    exclusive = false;
	}
    }

    if (OMPI_REGISTRY_XAND & mode) {  /* deal with XAND case */
	if (num_found == num_keys_entry) { /* found all, and nothing more */
	    return true;
	} else {  /* found either too many or not enough */
	    return false;
	}
    }

    if (OMPI_REGISTRY_XOR & mode) {  /* deal with XOR case */
	if (num_found > 0 && exclusive) {  /* found at least one and nothing not on list */
	    return true;
	} else {
	    return false;
	}
    }

    if (OMPI_REGISTRY_AND & mode) {  /* deal with AND case */
	if (num_found == num_keys_search) {  /* found all the required keys */
	    return true;
	} else {
	    return false;
	}
    }

    /* should be impossible situation, but just to be safe... */
    return false;
}

ompi_list_t *gpr_replica_test_internals(int level)
{
    ompi_list_t *test_results, *keylist;
    ompi_registry_internal_test_results_t *result;
    char name[30], name2[30];
    char *name3[30];
    int i, j;
    mca_gpr_replica_key_t segkey, key;
    mca_gpr_registry_segment_t *seg;
    mca_gpr_keytable_t *dict_entry;
    bool success;

    test_results = OBJ_NEW(ompi_list_t);

    /* create several test segments */
    success = true;
    result = OBJ_NEW(ompi_registry_internal_test_results_t);
    result->test = strdup("test-create-segment");
    for (i=0; i<5; i++) {
	sprintf(name, "test-def-seg%d", i);
	if (OMPI_SUCCESS != gpr_replica_define_segment(name)) {
	    success = false;
	}
    }
    if (success) {
	result->message = strdup("success");
    } else {
	result->message = strdup("failed");
    }
    ompi_list_append(test_results, &result->item);

    /* check that define key protects uniqueness */
    success = true;
    result = OBJ_NEW(ompi_registry_internal_test_results_t);
    result->test = strdup("test-define-key-uniqueness");
    for (i=0; i<5; i++) {
	sprintf(name, "test-def-seg%d", i);
	key = gpr_replica_define_key(name, NULL);
	if (MCA_GPR_REPLICA_KEY_MAX != key) { /* got an error */
	    success = false;
	}
    }
    if (success) {
	result->message = strdup("success");
    } else {
	result->message = strdup("failed");
    }
    ompi_list_append(test_results, &result->item);

    /* check ability to get key for a segment */
    success = true;
    result = OBJ_NEW(ompi_registry_internal_test_results_t);
    result->test = strdup("test-get-seg-key");
    for (i=0; i<5; i++) {
	sprintf(name, "test-def-seg%d", i);
	key = gpr_replica_get_key(name, NULL);
	if (MCA_GPR_REPLICA_KEY_MAX == key) { /* got an error */
	    success = false;
	}
    }
    if (success) {
	result->message = strdup("success");
    } else {
	result->message = strdup("failed");
    }
    ompi_list_append(test_results, &result->item);

    /* check the ability to find a segment */
    i = 2;
    sprintf(name, "test-def-seg%d", i);
    result = OBJ_NEW(ompi_registry_internal_test_results_t);
    result->test = strdup("test-find-seg");
    seg = gpr_replica_find_seg(name);
    if (NULL == seg) {
	asprintf(&result->message, "test failed with NULL returned: %s", name);
    } else {  /* locate key and check it */
	segkey = gpr_replica_get_key(name, NULL);
	if (segkey == seg->segment) {
	    result->message = strdup("success");
	} else {
	    asprintf(&result->message, "test failed: key %d seg %d", segkey, seg->segment);
	}
    }
    ompi_list_append(test_results, &result->item);

    /* check ability to define key within a segment */
    success = true;
    result = OBJ_NEW(ompi_registry_internal_test_results_t);
    result->test = strdup("test-define-key-segment");
    for (i=0; i<5 && success; i++) {
	sprintf(name, "test-def-seg%d", i);
	for (j=0; j<10 && success; j++) {
 	    sprintf(name2, "test-key%d", j);
	    key = gpr_replica_define_key(name, name2);
	    if (MCA_GPR_REPLICA_KEY_MAX == key) { /* got an error */
		success = false;
	    }
	}
    }
    if (success) {
	result->message = strdup("success");
    } else {
	result->message = strdup("failed");
    }
    ompi_list_append(test_results, &result->item);


    /* check ability to retrieve key within a segment */
    success = true;
    result = OBJ_NEW(ompi_registry_internal_test_results_t);
    result->test = strdup("test-get-key-segment");
    for (i=0; i<5 && success; i++) {
	sprintf(name, "test-def-seg%d", i);
	for (j=0; j<10 && success; j++) {
 	    sprintf(name2, "test-key%d", j);
	    key = gpr_replica_get_key(name, name2);
	    if (MCA_GPR_REPLICA_KEY_MAX == key) { /* got an error */
		success = false;
	    }
	}
    }
    if (success) {
	result->message = strdup("success");
    } else {
	result->message = strdup("failed");
    }
    ompi_list_append(test_results, &result->item);


    /* check ability to get dictionary entries */
    success = true;
    result = OBJ_NEW(ompi_registry_internal_test_results_t);
    result->test = strdup("test-get-dict-entry");
    /* first check ability to get segment values */
    for (i=0; i<5 && success; i++) {
	sprintf(name, "test-def-seg%d", i);
	dict_entry = gpr_replica_find_dict_entry(name, NULL);
	if (NULL == dict_entry) { /* got an error */
	    success = false;
	}
    }
    if (success) {
	result->message = strdup("success");
    } else {
	result->message = strdup("failed");
    }
    ompi_list_append(test_results, &result->item);

    if (success) { /* segment values checked out - move on to within a segment */
	result = OBJ_NEW(ompi_registry_internal_test_results_t);
	result->test = strdup("test-get-dict-entry-segment");
	for (i=0; i<5; i++) {
	    sprintf(name, "test-def-seg%d", i);
	    for (j=0; j<10; j++) {
		sprintf(name2, "test-key%d", j);
		dict_entry = gpr_replica_find_dict_entry(name, NULL);
		if (NULL == dict_entry) { /* got an error */
		    success = false;
		}
	    }
	}
	if (success) {
	    result->message = strdup("success");
	} else {
	    result->message = strdup("failed");
	}
	ompi_list_append(test_results, &result->item);
    }


    /* check ability to get key list */
    success = true;
    result = OBJ_NEW(ompi_registry_internal_test_results_t);
    result->test = strdup("test-get-keylist");
    for (i=0; i<5 && success; i++) {
	sprintf(name, "test-def-seg%d", i);
	for (j=0; j<10 && success; j++) {
 	    asprintf(&name3[j], "test-key%d", j);
	}
	name3[j] = NULL;
	keylist = gpr_replica_get_key_list(name, name3);
	if (0 >= ompi_list_get_size(keylist)) { /* error condition */
	    success = false;
	}
    }
    if (success) {
	result->message = strdup("success");
    } else {
	result->message = strdup("failed");
    }
    ompi_list_append(test_results, &result->item);

    /* check ability to empty segment */

    return test_results;
}
