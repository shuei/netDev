/* devDarwin.c */
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

// #define DEBUG // uncomment to enable debug output

#include <dbFldTypes.h>
#include <epicsExport.h>

#include "drvNetMpf.h"
#include "devNetDev.h"

#define DARWIN_MAX_CMND_LEN 256
#define DARWIN_TCP_PORT_SET 34150
#define DARWIN_TCP_PORT_MES 34151
#define DARWIN_NEEDS_SWAP (0x00ff != htons(0x00ff))
#define DARWIN_TIMEOUT ((TICK_PER_SECOND * 2)| MPF_FINETMO)

typedef struct {
    uint16_t port;
    uint8_t  comm_EF[DARWIN_MAX_CMND_LEN];
    uint8_t  comm_EL[DARWIN_MAX_CMND_LEN];
    uint8_t  comm_EB[DARWIN_MAX_CMND_LEN];
    int      unit;
    int      u1;
    int      u2;
    int      p1;
    int      p2;
} DARWIN;

static long darwin_parse_link(DBLINK *,
                              struct sockaddr_in *,
                              int *,
                              void *
                              );
static void *darwin_calloc(void);

static void *darwin_calloc(void)
{
    DARWIN *d = calloc(1, sizeof(DARWIN));
    if (!d) {
        errlogPrintf("devDarwin: calloc failed\n");
        return NULL;
    }
    return d;
}

#include        "devSoDarwin.c"
#include        "devChannelsDarwin.c"

//
// Link field parser for both command/response I/O and event driven I/O
//
static long darwin_parse_link(DBLINK *plink,
                              struct sockaddr_in *peer_addr,
                              int *option,
                              void *device
                              )
{
    DARWIN *d = (DARWIN *)device;
    // char terminator[2] = {'\n','\0'};
    char terminator[3] = "\r\n";
    char *protocol = NULL;
    char *unit  = NULL;
    char *addr  = NULL;
    char *route, *type, *lopt;
    uint8_t tmp_buf[DARWIN_MAX_CMND_LEN];
    uint8_t *src, *dst;
    char u1, u2;
    int p1, p2;

    if (parseLinkPlcCommon(plink,
                           peer_addr,
                           &protocol,
                           &route, // dummy
                           &unit,
                           &type,
                           &addr,
                           &lopt
                           )) {
        errlogPrintf("devDarwin: illegal input specification\n");
        return ERROR;
    }

    d->port = ntohs(peer_addr->sin_port);
    if (!d->port) {
        LOGMSG("devDarwin: port: 0x%04x\n", ntohs(peer_addr->sin_port));
        errlogPrintf("devDarwin: port not specified\n");
        return ERROR;
    }

    if (!addr && d->port == DARWIN_TCP_PORT_MES) {
        errlogPrintf("devDarwin: no command specified\n");
        return ERROR;
    }

    if (unit) {
        if (sscanf(unit, "%d", &d->unit) != 1) {
            errlogPrintf("devDarwin: can't get unit\n");
            return ERROR;
        }
    }

    switch (d->port) {
    case DARWIN_TCP_PORT_SET:
        // nothing to do
        break;
    case DARWIN_TCP_PORT_MES:
        for (src = (uint8_t *)addr, dst = &tmp_buf[0];
             (*src != '\0') && (dst < tmp_buf + sizeof(tmp_buf)); ) {
            if (*src == ' ') {
                src++;
            } else {
                *dst++ = *src++;
            }
            tmp_buf[sizeof(tmp_buf) - 1] = '\0';
        }

        if (sscanf((char *)tmp_buf, "%c%d,%c%d", &u1, &p1, &u2, &p2) != 4) {
            errlogPrintf("devDarwin: can't get parms from \"%s\"\n", addr);
            return ERROR;
        }

        d->u1 = u1;
        d->u2 = u2;
        d->p1 = p1;
        d->p2 = p2;

        LOGMSG("p1:%d, p2:%d\n", d->p1, d->p2);
#ifdef vxWorks
        sprintf(d->comm_EF, "EF ,%c%02d,%c%02d%s", u1, p1, u2, p2, terminator);
        sprintf(d->comm_EL, "EL%c%02d,%c%02d%s",   u1, p1, u2, p2, terminator);
        sprintf(d->comm_EB, "EB %s", terminator);
#else
        snprintf((char *)d->comm_EF, DARWIN_MAX_CMND_LEN - 1, "EF ,%c%02d,%c%02d%s", u1, p1, u2, p2, terminator);
        snprintf((char *)d->comm_EL, DARWIN_MAX_CMND_LEN - 1, "EL%c%02d,%c%02d%s",   u1, p1, u2, p2, terminator);
        snprintf((char *)d->comm_EB, DARWIN_MAX_CMND_LEN - 1, "EB %s", terminator);
#endif
        break;
    default:
        errlogPrintf("devDarwin: unsupported port(%d) specified\n",
                     ntohs(peer_addr->sin_port));
        return ERROR;
    }

    return OK;
}
