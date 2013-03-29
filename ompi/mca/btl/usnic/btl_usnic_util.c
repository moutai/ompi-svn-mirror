/*
 * Copyright (c) 2012 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>
#include <infiniband/verbs.h>

#include "ompi/constants.h"

#include "btl_usnic_util.h"
#include "btl_usnic_if.h"


void ompi_btl_usnic_sprintf_mac(char *out, const uint8_t mac[6])
{
    snprintf(out, 32, "%02x:%02x:%02x:%02x:%02x:%02x", 
             mac[0],
             mac[1],
             mac[2],
             mac[3],
             mac[4],
             mac[5]);
}


int ompi_btl_usnic_find_ip(ompi_btl_usnic_module_t *module, uint8_t mac[6])
{
    int i;
    uint8_t localmac[6];
    char addr_string[32], mac_string[32];
    struct sockaddr sa;
    struct sockaddr_in *sai;

    /* Loop through all IP interfaces looking for the one with the
       right MAC */
    for (i = btl_usnic_opal_ifbegin(); i != -1; i = btl_usnic_opal_ifnext(i)) {
        if (OPAL_SUCCESS == btl_usnic_opal_ifindextomac(i, localmac)) {

            /* Is this the MAC I'm looking for? */
            if (0 != memcmp(mac, localmac, 6)) {
                continue;
            }

            /* Yes, it is! */
            if (OPAL_SUCCESS != btl_usnic_opal_ifindextoname(i, module->if_name, 
                                                   sizeof(module->if_name)) ||
                OPAL_SUCCESS != btl_usnic_opal_ifindextoaddr(i, &sa, sizeof(sa)) ||
                OPAL_SUCCESS != btl_usnic_opal_ifindextomask(i, &module->if_cidrmask,
                                                   sizeof(module->if_cidrmask)) ||
                OPAL_SUCCESS != btl_usnic_opal_ifindextomac(i, module->if_mac)) {
                continue;
            }

            sai = (struct sockaddr_in *) &sa;
            memcpy(&module->if_ipv4_addr, &sai->sin_addr, 4);

            /* Save this information to my local address field on the
               module so that it gets sent in the modex */
            module->addr.ipv4_addr = module->if_ipv4_addr;
            module->addr.cidrmask = module->if_cidrmask;

            inet_ntop(AF_INET, &(module->if_ipv4_addr),
                      addr_string, sizeof(addr_string));
            ompi_btl_usnic_sprintf_mac(mac_string, mac);
            opal_output_verbose(5, mca_btl_base_verbose,
                                "btl:usnic: found USNIC device corresponds to IP device %s, %s/%d, MAC %s",
                                module->if_name, addr_string, module->if_cidrmask, 
                                mac_string);
            return OMPI_SUCCESS;
        }
    }

    return OMPI_ERR_NOT_FOUND;
}


/*
 * Reverses the encoding done in usnic_main.c:usnic_mac_to_gid() in
 * the usnic.ko kernel code.
 */
void ompi_btl_usnic_gid_to_mac(union ibv_gid *gid, uint8_t mac[6])
{
    mac[0] = gid->raw[8] ^ 2;
    mac[1] = gid->raw[9];
    mac[2] = gid->raw[10];
    mac[3] = gid->raw[13];
    mac[4] = gid->raw[14];
    mac[5] = gid->raw[15];
}
