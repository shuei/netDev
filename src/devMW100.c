/* devMW100.c */
/****************************************************************************
 *                         COPYRIGHT NOTIFICATION
 *
 * Copyright (c) All rights reserved
 *
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 ****************************************************************************/
/* Author: Jun-ichi Odagiri */

#include <epicsExport.h>
#include <dbFldTypes.h>

#include "drvNetMpf.h"
#include "devNetDev.h"

#define MW100_FDCMD_BUFSIZE  256
#define MW100_SERVER_PORT    34318
#define MW100_TIMEOUT ((TICK_PER_SECOND * 2)| MPF_FINETMO)
#define MW100_FIRST_CHANNEL  1
#define MW100_LAST_CHANNEL   60

typedef struct {
    uint8_t  com_FD[MW100_FDCMD_BUFSIZE];
    int      com_len;
    int      p1;
    int      p2;
    int      noch;
} MW100;

LOCAL long MW100_parse_link(DBLINK *,
                            struct sockaddr_in *,
                            int *,
                            void *
                            );

LOCAL void *MW100_calloc(void);

LOCAL void *MW100_calloc(void)
{
    MW100 *d = calloc(1, sizeof(MW100));
    if (!d) {
        errlogPrintf("devMW100: calloc failed\n");
        return NULL;
    }

    return d;
}

#include "devSoMW100.c"
#include "devChansMW100.c"

//
// Link field parser for both command/response I/O and event driven I/O
//
LOCAL long MW100_parse_link(DBLINK *plink,
                            struct sockaddr_in *peer_addr,
                            int *option,
                            void *device
                            )
{
    MW100 *d = (MW100 *)device;
    char terminator[3] = "\r\n";
    char *protocol = NULL;
    char *unit  = NULL;
    char *addr  = NULL;
    char *route, *type, *lopt;

    if (parseLinkPlcCommon(plink,
                           peer_addr,
                           &protocol,
                           &route, // dummy
                           &unit,
                           &type,
                           &addr,
                           &lopt
                           )) {
        errlogPrintf("devMW100: illegal input specification\n");
        return ERROR;
    }

    if (!peer_addr->sin_port) {
        peer_addr->sin_port = htons(MW100_SERVER_PORT);
        LOGMSG("devMW100: port: 0x%04x\n", ntohs(peer_addr->sin_port));
    }

    if (addr) {
        uint8_t *src, *dst;
        uint8_t tmp_buf[MW100_FDCMD_BUFSIZE];
        for (src = (uint8_t *)addr, dst = &tmp_buf[0];
             (*src != '\0') && (dst < tmp_buf + sizeof(tmp_buf));
             ) {
            if (*src == ' ') {
                src++;
            } else {
                *dst++ = *src++;
            }
            tmp_buf[sizeof(tmp_buf) - 1] = '\0';
        }

        int p1, p2;
        if (sscanf((char *)tmp_buf, "%d,%d", &p1, &p2) != 2) {
            errlogPrintf("devMW100: can't get parms from \"%s\"\n", addr);
            return ERROR;
        }

        d->p1 = p1;
        d->p2 = p2;

        if ((p1 < MW100_FIRST_CHANNEL || p1 > MW100_LAST_CHANNEL) ||
            (p2 < MW100_FIRST_CHANNEL || p2 > MW100_LAST_CHANNEL) ||
            (p1 > p2)) {
            errlogPrintf("devMW100: illegal parameter(s) (p1:%d, p2:%d)\n", p1, p2);
            return ERROR;
        }

        LOGMSG("p1:%d, p2:%d\n", d->p1, d->p2);

        d->noch = p2 - p1 + 1;

        sprintf((char *)d->com_FD, "FD0,%03d,%03d%s", p1, p2, terminator);

        d->com_len = strlen((char *)d->com_FD);
    }

    return OK;
}
