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

/**
 * @file
 *
 * Setup command line options for the Open Run Time Environment
 */

#include "orte_config.h"

#include "util/cmd_line.h"

#include "runtime/runtime.h"

void orte_cmd_line_setup(ompi_cmd_line_t *cmd_line)
{
    /* setup the rte command line arguments */
    ompi_cmd_line_make_opt3(cmd_line,  /* read in orte_parse_daemon_cmd_line */
			    's', "seed", "seed", 0,
			   "Set the daemon seed to true.");

    ompi_cmd_line_make_opt3(cmd_line,  /* read in orte_parse_daemon_cmd_line */
                '\0', "bootproxy", "bootproxy", 0,
                "Act as bootproxy");

    ompi_cmd_line_make_opt3(cmd_line,  /* read in orte_parse_cmd_line */
			    'u', "universe", "universe", 1,
			   "Specify the name/host of the ORTE universe");

    ompi_cmd_line_make_opt3(cmd_line,  /* read in orte_parse_cmd_line */
               '\0', "universe_uri", "universe_uri", 1,
               "OOB contact info for seed of this universe");

    ompi_cmd_line_make_opt3(cmd_line,  /* read in orte_parse_cmd_line */
			    '\0', "tmpdir", "tmpdir", 1,
			   "Specify the prefix for the ORTE session directory");

    ompi_cmd_line_make_opt3(cmd_line,  /* read in orte_parse_daemon_cmd_line */
			   '\0', "persistent", "persistent", 0,
			   "Universe is to be persistent");

    ompi_cmd_line_make_opt3(cmd_line,  /* read in orte_parse_daemon_cmd_line */
			    's', "console", "console", 0,
			   "Provide a console for user interaction");

    ompi_cmd_line_make_opt3(cmd_line,  /* read in orte_parse_daemon_cmd_line */
			    'f', "script", "script", 1,
			   "Read commands from script file");

    ompi_cmd_line_make_opt3(cmd_line,  /* read in orte_parse_daemon_cmd_line */
			    '\0', "scope", "scope", 1,
			   "Scope of this universe");

    ompi_cmd_line_make_opt3(cmd_line,  /* read in orte_parse_cmd_line */
			    '\0', "nsreplica", "nsreplica", 1,
			    "OOB contact info for name server replica assigned to this process");

    ompi_cmd_line_make_opt3(cmd_line,  /* read in orte_parse_cmd_line */
			    '\0', "gprreplica", "gprreplica", 1,
			    "OOB contact info for GPR replica assigned to this process");
}
