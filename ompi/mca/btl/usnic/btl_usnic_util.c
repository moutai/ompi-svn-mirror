/*
 * Copyright (c) 2013 Cisco Systems, Inc.  All rights reserved.
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

void
ompi_btl_usnic_dump_hex(uint8_t *addr, int len)
{
    char buf[128];
    int i;
    char *p;
    uint32_t sum=0;

    p = buf;
    for (i=0; i<len; ++i) {
        p += sprintf(p, "%02x ", addr[i]);
        sum += addr[i];
        if ((i&15) == 15) {
            opal_output(0, "%4x: %s\n", i&~15, buf);
            p = buf;
        }
    }
    if ((i&15) != 0) {
        opal_output(0, "%4x: %s\n", i&~15, buf);
    }
    /*opal_output(0, "buffer sum = %x\n", sum); */
}


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


void ompi_btl_usnic_sprintf_gid_mac(char *out, union ibv_gid *gid)
{
    uint8_t mac[6];
    ompi_btl_usnic_gid_to_mac(gid, mac);
    ompi_btl_usnic_sprintf_mac(out, mac);
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
                OPAL_SUCCESS != btl_usnic_opal_ifindextomac(i, module->if_mac) ||
                OPAL_SUCCESS != btl_usnic_opal_ifindextomtu(i, &module->if_mtu)) {
                continue;
            }

            sai = (struct sockaddr_in *) &sa;
            memcpy(&module->if_ipv4_addr, &sai->sin_addr, 4);

            /* Save this information to my local address field on the
               module so that it gets sent in the modex */
            module->local_addr.ipv4_addr = module->if_ipv4_addr;
            module->local_addr.cidrmask = module->if_cidrmask;

            /* Since verbs doesn't offer a way to get standard
               Ethernet MTUs (as of libibverbs 1.1.5, the MTUs are
               enums, and don't inlcude values for 1500 or 9000), look
               up the MTU in the corresponding enic interface.
               Subtract 40 off the MTU value so that we provide enough
               space for the GRH on the remote side. */
            module->if_mtu -= 40;
            module->local_addr.mtu = module->if_mtu;

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
 *
 * Got this scheme from Mellanox RoCE; Emulex did the same thing.  So
 * we followed convention.
 * http://www.mellanox.com/related-docs/prod_software/RoCE_with_Priority_Flow_Control_Application_Guide.pdf
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
