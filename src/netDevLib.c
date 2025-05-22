/* netDevLib.c */
/****************************************************************************
 *                         COPYRIGHT NOTIFICATION
 *
 * Copyright (c) All rights reserved
 *
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 ****************************************************************************/

//
#include <dbFldTypes.h>
#include <alarm.h>
#include <epicsExport.h>
#include <recGbl.h>

//
#include "drvNetMpf.h"
#include "devNetDev.h"

//
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

//
// Routines for signed integer data transfer
//
static long toCharVal   (int8_t  *, int, void *, int, int, int);
static long toShortVal  (int16_t *, int, void *, int, int, int);
static long toLongVal   (int32_t *, int, void *, int, int, int);
static long fromCharVal (void *, int8_t *,  int, int, int, int);
static long fromShortVal(void *, int16_t *, int, int, int, int);
static long fromLongVal (void *, int32_t *, int, int, int, int);

//
// Routines for unsigned integer data transfer
//
static long toUcharVal   (uint8_t  *, int, void *, int, int, int);
static long toUshortVal  (uint16_t *, int, void *, int, int, int);
static long toUlongVal   (uint32_t *, int, void *, int, int, int);
static long fromUcharVal (void *, uint8_t  *, int, int, int, int);
static long fromUshortVal(void *, uint16_t *, int, int, int, int);
static long fromUlongVal (void *, uint32_t *, int, int, int, int);

//
// Routines for floating point data transfer
//
static long toFloatVal   (float *, int, void *, int, int, int);
static long toDoubleVal  (double *, int, void *, int, int, int);
static long fromFloatVal (void *, float *, int, int, int, int);
static long fromDoubleVal(void *, double *, int, int, int, int);

//
//
//
#define BCDMAX_BCD 39321 // 0x9999
#define BCDMAX_INT  9999 // 0x9999

#define BCDMIN_BCD     0 // 0x9999
#define BCDMIN_INT     0 //

//
//
//
int netDevDebug = 0;

//
//
//
void swap_bytes(void *ptr, int num)
{
    uint16_t *p = ptr;
    for (int i = 0; i < num; i++) {
        p[i] = (uint16_t)(((p[i]>>8)&0x00ff) | ((p[i]&0x00ff)<<8));
    }
}

#if 0
static void swap_dword(void *ptr, int num)
{
    uint32_t *p = ptr;
    for (int i = 0; i < num; i++) {
        p[i] = (uint32_t)(((p[i]>>16)&0x0000ffff) | ((p[i]&0x0000ffff)<<16));
    }
}
#endif

static void dump(void *from, int fsiz, void *to, int tsiz, int ndata, char *file, const char *func)
{
    const int skip = 4;
    for (int i = 0; i < ndata; i++) {
        if (skip<=i && i+skip<ndata) {
            continue;
        }

        printf("%s: %-13s [%4d", file, func, i+1);
        if (fsiz == 1) {
            uint8_t *p = from;
            printf(" 0x%02x", p[i]);
        } else if (fsiz == 2) {
            uint16_t *p = from;
            printf(" 0x%04x", p[i]);
        } else if (fsiz == 4) {
            uint32_t *p = from;
            printf(" 0x%08x", p[i]);
        } else if (fsiz == 8) {
            uint64_t *p = from;
            printf(" 0x%016"PRIx64, p[i]);
        }

        printf(" =>");

        if (tsiz == 1) {
            uint8_t *p = to;
            printf(" 0x%02x", p[i]);
        } else if (tsiz == 2) {
            uint16_t *p = to;
            printf(" 0x%04x", p[i]);
        } else if (tsiz == 4) {
            uint32_t *p = to;
            printf(" 0x%08x", p[i]);
        } else if (tsiz == 8) {
            uint64_t *p = to;
            printf(" 0x%016"PRIx64, p[i]);
        }

        printf("]\n");
    }
}

//
// Link field parser for PLCs
//
long parseLinkPlcCommon(DBLINK *plink,
                        struct sockaddr_in *peer_addr,
                        char **protocol,
                        char **route,
                        char **unit,
                        char **type,
                        char **addr,
                        char **lopt
                        )
{
    char *pc0      = NULL;
    char *pc1      = NULL;
    char *pc2      = NULL;
    char *pc3      = NULL;

    LOGMSG("devNetDev: %s(%8p,%8p,%8p,%8p,%8p,%8p)\n", __func__, plink, peer_addr, route, unit, type, addr);

    // hostname[{routing_path}][:unit]#[type:]addr[&option]
    // hostname[:unit]#[type:]addr

    size_t size = strlen(plink->value.instio.string) + 1;
    if (size > MAX_INSTIO_STRING) {
        errlogPrintf("devNetDev: instio.string is too long\n");
        return ERROR;
    }

    char *buf = calloc(1, size);
    if (!buf) {
        errlogPrintf("devNetDev: can't calloc for buf\n");
        return ERROR;
    }

    strcpy(buf, plink->value.instio.string);
    buf[size - 1] = '\0';
    LOGMSG("devNetDev: buf[%zd]: %s\n", size, buf);

    pc0 = strchr(buf, '&');
    if (pc0) {
        *pc0++ = '\0';
        *lopt = pc0;
        LOGMSG("devNetDev: lopt: %s\n", *lopt);
    }
    //pc0 = NULL;

    pc0 = strchr(buf, '(');
    if (pc0) {
        *pc0++ = '\0';
        char *port = pc0;
        pc1 = strchr(pc0, ')');
        if (!pc1) {
            LOGMSG("devNetDev: pc0:%8p, pc1:%8p\n",pc0, pc1);
            errlogPrintf("devNetDev: can't get port\n");
            return ERROR;
        }
        *pc1++ = '\0';

        //pc0 = NULL;
        pc0 = strchr(port, ':');
        if (pc0) {
            *pc0++ = '\0';
            *protocol = pc0;

            LOGMSG("devNetDev: protocol: %s\n", *protocol);
        }

        if (*port != '\0') { // it was ':'
            LOGMSG("devNetDev: string port: %s\n", port);

            unsigned int iport;
            if (strncmp(port, "0x", 2) == 0) {
                if (sscanf(port, "%x", &iport) != 1) {
                    errlogPrintf("devNetDev: can't get service port\n");
                    return ERROR;
                }
                LOGMSG("devNetDev: port in HEX: 0x%04x\n", iport);
            } else {
                if (sscanf(port, "%d", &iport) != 1) {
                    errlogPrintf("devNetDev: can't get service port\n");
                    return ERROR;
                }
                LOGMSG("devNetDev: port in DEC: %d\n", iport);
            }
            peer_addr->sin_port = htons(iport);
            LOGMSG("devNetDev: port: 0x%04x\n", ntohs(peer_addr->sin_port));
        }
    }

    if (pc1) {
        pc0 = pc1;
    } else {
        pc0 = buf;
    }

    //pc1 = NULL;
    pc1 = strchr(pc0, '#');
    pc2 = strchr(pc0, ':');

    if (!pc1) {
        LOGMSG("devNetDev: no device address specified\n");
        *unit = NULL;
        *type = NULL;
        *addr = NULL;
        pc1 = &buf[size - 1];
        pc2 = &buf[size - 1];
        goto GET_HOSTNAME;
    } else {
        *pc1++ = '\0';
    }

    if (!pc2) {
        // no unit, no type ( hostname#addr )
        *unit = NULL;
        *type = NULL;
        *addr = pc1;
        LOGMSG("devNetDev: no unit, no type\n");
    } else {
        *pc2++ = '\0';
        if (pc2 > pc1) {
            // no unit, but type exists ( hostname#type:addr )
            *unit = NULL;
            *type = pc1;
            *addr = pc2;
            LOGMSG("devNetDev: no unit, type exists\n");
        } else {
            // unit exists. don't know about type ( hostname:unit#[type:]addr )
            *unit = pc2;
            pc3 = strchr(pc1, ':');
            if (!pc3) {
                // unit exists, but no type ( hostname:unit#addr )
                *type = NULL;
                *addr = pc1;
                LOGMSG("devNetDev: unit exists, but no type\n");
            } else {
                // both unit and type exist ( hostname:unit#type:addr )
                *pc3++ = '\0';
                *type = pc1;
                *addr = pc3;
                LOGMSG("devNetDev: both unit and type exist\n");
            }
        }
    }

GET_HOSTNAME:
    ; // null-statement to avoid label followed by declaration
    char *hostname = buf;

    if (netDevGetHostId(hostname, &peer_addr->sin_addr.s_addr)) {
        errlogPrintf("devNetDev: can't get hostid\n");
        return ERROR;
    }
    LOGMSG("devNetDev: hostid: 0x%08x\n", peer_addr->sin_addr.s_addr);

    return OK;
}

//
// Copy values from receive buffer to record
//
long toRecordVal(void *to,
                 int   noff,
                 int   ftvl,
                 void *from,
                 int   width,
                 int   ndata,
                 int   swap
                 )
{
    //DEBUG
    //if (netDevDebug>0) {
    //    printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    //}

    switch (ftvl) {
    case DBF_CHAR:
        return toCharVal(to, noff, from, width, ndata, swap);
    case DBF_SHORT:
        return toShortVal(to, noff, from, width, ndata, swap);
    case DBF_LONG:
        return toLongVal(to, noff, from, width, ndata, swap);
    case DBF_UCHAR:
        return toUcharVal(to, noff, from, width, ndata, swap);
    case DBF_USHORT:
        return toUshortVal(to, noff, from, width, ndata, swap);
    case DBF_ULONG:
        return toUlongVal(to, noff, from, width, ndata, swap);
    case DBF_FLOAT:
        return toFloatVal(to, noff, from, width, ndata, swap);
    case DBF_DOUBLE:
        return toDoubleVal(to, noff, from, width, ndata, swap);
    default:
        errlogPrintf("devNetDev: %s: unsupported FTVL: %d\n", __func__, ftvl);
        return ERROR;
    }

    return OK;
}

//
// To Char record buffer
//
static long toCharVal(int8_t *to,
                      int noff,
                      void *from,
                      int width,
                      int ndata,
                      int swap
                      )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    }

    to += noff;

    if (0) {
        //
    } else if (width == 2) {
        int8_t *p = from;
        for (int i = 0; i < ndata; i++) {
            //DEBUG
            //if (netDevDebug>1) {
            //    printf("%d %d 0x%08x 0x%08x\n", i, p[i], p[i], p[i]&0xff00);
            //}
#if 1
            if ((p[i] & 0xff00) /*&&
                (p[i] & 0xff00) != 0xff00 */) { // need check
                errlogPrintf("%s [%s:%d] illegal data conversion: width=%d ndata=%d i=%d from=%d\n", __func__, __FILE__, __LINE__, width, ndata, i, p[i]);
                return ERROR;
            }
#endif
            to[i] = p[i];
        }

        if (swap) {
            swap_bytes(to, ndata);
        }
    } else if (width == 1) {
        int8_t *p = from;
        for (int i = 0; i < ndata; i++) {
            to[i] = p[i];
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    //DEBUG
    if (netDevDebug>0) {
        dump(from, width, to, sizeof(*to), ndata, __FILE__, __func__);
    }

    return OK;
}

//
// To Short record buffer
//
static long toShortVal(int16_t *to,
                       int noff,
                       void *from,
                       int width,
                       int ndata,
                       int swap
                       )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    }

    to += noff;

    if (0) {
        //
    } else if (width == 2) {
        int16_t *p = from;
        for (int i = 0; i < ndata; i++) {
            to[i] = p[i];
        }

        if (swap) {
            swap_bytes(to, ndata);
        }
    } else if (width == 1) {
        int8_t *p = from;
        for (int i = 0; i < ndata; i++) {
            to[i] = p[i];
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    //DEBUG
    if (netDevDebug>0) {
        dump(from, width, to, sizeof(*to), ndata, __FILE__, __func__);
    }

    return OK;
}

//
// To Long record buffer
//
static long toLongVal(int32_t *to,
                      int noff,
                      void *from,
                      int width,
                      int ndata,
                      int swap
                      )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    }

    to += noff;

    if (0) {
        //
    } else if (width == 4) {
        uint16_t *p = from;
        for (int i = 0; i < ndata; i++) {
            to[i] = (p[i*2+1]<<16) | p[i*2];
        }

        if (swap) {
            swap_bytes(to, 2*ndata);
        }
    } else if (width == 2) {
        int16_t *p = from;
        for (int i = 0; i < ndata; i++) {
            to[i] = p[i];
        }

        if (swap) {
            swap_bytes(to, 2*ndata); // record's buffer size is 2*ndata
        }
    } else if (width == 1) {
        int8_t *p = from;
        for (int i = 0; i < ndata; i++) {
            to[i] = p[i];
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    //DEBUG
    if (netDevDebug>0) {
        dump(from, width, to, sizeof(*to), ndata, __FILE__, __func__);
    }

    return OK;
}

//
// To Uchar record buffer
//
static long toUcharVal(uint8_t *to,
                       int noff,
                       void *from,
                       int width,
                       int ndata,
                       int swap
                       )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    }

    to += noff;

    if (0) {
        //
    } else if (width == 2) {
        uint16_t *p = from;
        for (int i = 0; i < ndata; i++) {
            //DEBUG
            //if (netDevDebug>0) {
            //    printf("%d %d 0x%08x 0x%08x\n", i, p[i], p[i], p[i]&0xff00);
            //}
#if 1
            if ((p[i] & 0xff00) /*&&
                (p[i] & 0xff00) != 0xff00 */) { // need check
                errlogPrintf("%s [%s:%d] illegal data conversion: width=%d ndata=%d i=%d from=%d\n", __func__, __FILE__, __LINE__, width, ndata, i, p[i]);
                return ERROR;
            }
#endif
            to[i] = p[i];
        }

        if (swap) {
            swap_bytes(to, ndata);
        }
    } else if (width == 1) {
        uint8_t *p = from;

        for (int i = 0; i < ndata; i++) {
            to[i] = p[i];
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    //DEBUG
    if (netDevDebug>0) {
        dump(from, width, to, sizeof(*to), ndata, __FILE__, __func__);
    }

    return OK;
}

//
// To Ushort record buffer
//
static long toUshortVal(uint16_t *to,
                        int noff,
                        void *from,
                        int width,
                        int ndata,
                        int swap
                        )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    }

    to += noff;

    if (0) {
        //
    } else if (width == 2) {
        uint16_t *p = from;
        for (int i = 0; i < ndata; i++) {
            to[i] = p[i];
        }

        if (swap) {
            swap_bytes(to, ndata);
        }
    } else if (width == 1) {
        uint8_t *p = from;
        for (int i = 0; i < ndata; i++) {
            to[i] = p[i];
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    //DEBUG
    if (netDevDebug>0) {
        dump(from, width, to, sizeof(*to), ndata, __FILE__, __func__);
    }

    return OK;
}

//
// To Ulong record buffer
//
static long toUlongVal(uint32_t *to,
                       int noff,
                       void *from,
                       int width,
                       int ndata,
                       int swap
                       )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    }

    to += noff;

    if (0) {
        //
    } else if (width == 4) {
        int16_t *p = from;
        for (int i = 0; i < ndata; i++) {
            to[i] = (p[i*2+1]<<16) | p[i*2];
        }

        if (swap) {
            swap_bytes(to, 2*ndata);
        }
    } else if (width == 2) {
        uint16_t *p = from;
        for (int i = 0; i < ndata; i++) {
            to[i] = p[i];
        }

        if (swap) {
            swap_bytes(to, ndata);
        }
    } else if (width == 1) {
        uint8_t *p = from;
        for (int i = 0; i < ndata; i++) {
            to[i] = p[i];
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    //DEBUG
    if (netDevDebug>0) {
        dump(from, width, to, sizeof(*to), ndata, __FILE__, __func__);
    }

    return OK;
}

//
// To Float record buffer
//
static long toFloatVal(float *to,
                       int noff,
                       void *from,
                       int width,
                       int ndata,
                       int swap
                       )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    }

    to += noff;

    if (0) {
        //
    } else if (width == 4) {
        uint16_t *p = from;
        for (int i = 0; i < ndata; i++) {
            uint32_t lval = (p[i*2+1]<<16) | p[i*2];
            memcpy(&to[i], &lval, sizeof(float));
        }

        if (swap) {
            swap_bytes(to, 2*ndata);
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    //DEBUG
    if (netDevDebug>0) {
        dump(from, width, to, sizeof(*to), ndata, __FILE__, __func__);
    }

    return OK;
}

//
// To Double record buffer
//
static long toDoubleVal(double *to,
                        int noff,
                        void *from,
                        int width,
                        int ndata,
                        int swap
                        )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    }

    to += noff;

    if (0) {
        //
    } else if (width == 8) {
        uint16_t *p = from;
        for (int i = 0; i < ndata; i++) {
            uint64_t p0 = p[i*4+0];
            uint64_t p1 = p[i*4+1];
            uint64_t p2 = p[i*4+2];
            uint64_t p3 = p[i*4+3];
            uint64_t lval = (p3<<48) | (p2<<32) | (p1<<16) | p0;
            memcpy(&to[i], &lval, sizeof(lval));
        }

        if (swap) {
            swap_bytes(to, 4*ndata);
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    //DEBUG
    if (netDevDebug>0) {
        dump(from, width, to, sizeof(*to), ndata, __FILE__, __func__);
    }

    return OK;
}

//
// Copy values from record to send buffer
//
long fromRecordVal(void *to,
                   int   width,
                   void *from,
                   int   noff,
                   int   ftvl,
                   int   ndata,
                   int   swap
                   )
{
    //DEBUG
    //if (netDevDebug>0) {
    //    printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    //}

    switch (ftvl) {
    case DBF_CHAR:
        return fromCharVal(to, from, noff, width, ndata, swap);
    case DBF_SHORT:
        return fromShortVal(to, from, noff, width, ndata, swap);
    case DBF_LONG:
        return fromLongVal(to, from, noff, width, ndata, swap);
    case DBF_UCHAR:
        return fromUcharVal(to, from, noff, width, ndata, swap);
    case DBF_USHORT:
        return fromUshortVal(to, from, noff, width, ndata, swap);
    case DBF_ULONG:
        return fromUlongVal(to, from, noff, width, ndata, swap);
    case DBF_FLOAT:
        return fromFloatVal(to, from, noff, width, ndata, swap);
    case DBF_DOUBLE:
        return fromDoubleVal(to, from, noff, width, ndata, swap);
    default:
        errlogPrintf("devNetDev: %s: unsupported FTVL: %d\n", __func__, ftvl);
        return ERROR;
    }

    return OK;
}

//
// From Char record buffer
//
static long fromCharVal(void *to,
                        int8_t *from,
                        int noff,
                        int width,
                        int ndata,
                        int swap
                        )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    }

    from += noff;

    if (0) {
        //
    } else if (width == 2) {
        int16_t *p = to;
        for (int i = 0; i < ndata; i++) {
            p[i] = from[i];
        }

        if (swap) {
            swap_bytes(to, ndata);
        }
    } else if (width == 1) {
        int8_t *p = to;
        for (int i = 0; i < ndata; i++) {
            p[i] = from[i];
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    //DEBUG
    if (netDevDebug>0) {
        dump(from, sizeof(*from), to, width, ndata, __FILE__, __func__);
    }

    return OK;
}

//
// From Short record buffer
//
static long fromShortVal(void *to,
                         int16_t *from,
                         int noff,
                         int width,
                         int ndata,
                         int swap
                         )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    }

    from += noff;

    if (0) {
        //
    } else if (width == 2) {
        int16_t *p = to;
        for (int i = 0; i < ndata; i++) {
            p[i] = from[i];
        }

        if (swap) {
            swap_bytes(to, ndata);
        }
    } else if (width == 1) {
        int8_t *p = to;
        for (int i = 0; i < ndata; i++) {
            //DEBUG
            //if (netDevDebug>0) {
            //    printf("%d %d 0x%08x 0x%08x\n", i, from[i], from[i], from[i]&0xff00);
            //}
#if 1
            if ((from[i] & 0xff00) /*&&
                (from[i] & 0xff00) != 0xff00 */) { // need check
                errlogPrintf("%s [%s:%d] illegal data conversion: width=%d ndata=%d i=%d from=%d\n", __func__, __FILE__, __LINE__, width, ndata, i, from[i]);
                return ERROR;
            }
#endif
            p[i] = from[i];
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    //DEBUG
    if (netDevDebug>0) {
        dump(from, sizeof(*from), to, width, ndata, __FILE__, __func__);
    }

    return OK;
}

//
// From Long record buffer
//
static long fromLongVal(void *to,
                        int32_t *from,
                        int noff,
                        int width,
                        int ndata,
                        int swap
                        )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    }

    from += noff;

    if (0) {
        //
    } else if (width == 4) {
        uint16_t *p = to;
        for (int i = 0; i < ndata; i++) {
            p[2*i  ] = from[i]>> 0;
            p[2*i+1] = from[i]>>16;
        }

        if (swap) {
            swap_bytes(to, 2*ndata);
        }
    } else if (width == 2) {
        int16_t *p = to;
        for (int i = 0; i < ndata; i++) {
            //DEBUG
            //if (netDevDebug>0) {
            //    printf("%d %d 0x%08x 0x%08x\n", i, from[i], from[i], from[i]&0xffff0000);
            //}
#if 1
            if ((from[i] & 0xffff0000) /* &&
                (from[i] & 0xffff0000) != 0xffff0000 */) { // need check
                errlogPrintf("%s [%s:%d] illegal data conversion: width=%d ndata=%d i=%d from=%d\n", __func__, __FILE__, __LINE__, width, ndata, i, from[i]);
                return ERROR;
            }
#endif
            p[i] = from[i];
        }

        if (swap) {
            swap_bytes(to, ndata);
        }
    } else if (width == 1) {
        int8_t *p = to;
        for (int i = 0; i < ndata; i++) {
            //DEBUG
            //if (netDevDebug>0) {
            //    printf("%d %d 0x%08x 0x%08x\n", i, from[i], from[i], from[i]&0xffffff00);
            //}
#if 1
            if ((from[i] & 0xffffff00) /* &&
                (from[i] & 0xffffff00) != 0xffffff00 */) { // need check
                errlogPrintf("%s [%s:%d] illegal data conversion: width=%d ndata=%d from=%d\n", __func__, __FILE__, __LINE__, width, ndata, *from);
                return ERROR;
            }
#endif
            p[i] = from[i];
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    //DEBUG
    if (netDevDebug>0) {
        dump(from, sizeof(*from), to, width, ndata, __FILE__, __func__);
    }

    return OK;
}

//
// From Uchar record buffer
//
static long fromUcharVal(void *to,
                         uint8_t *from,
                         int noff,
                         int width,
                         int ndata,
                         int swap
                         )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    }

    from += noff;

    if (0) {
        //
    } else if (width == 2) {
        uint16_t *p = to;
        for (int i = 0; i < ndata; i++) {
            p[i] = from[i];
        }

        if (swap) {
            swap_bytes(to, ndata);
        }
    } else if (width == 1) {
        uint8_t *p = to;
        for (int i = 0; i < ndata; i++) {
            p[i] = from[i];
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    //DEBUG
    if (netDevDebug>0) {
        dump(from, sizeof(*from), to, width, ndata, __FILE__, __func__);
    }

    return OK;
}

//
// From Ushort record buffer
//
static long fromUshortVal(void *to,
                          uint16_t *from,
                          int noff,
                          int width,
                          int ndata,
                          int swap
                          )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    }

    from += noff;

    if (0) {
        //
    } else if (width == 2) {
        uint16_t *p = to;
        for (int i = 0; i < ndata; i++) {
            p[i] = from[i];
        }

        if (swap) {
            swap_bytes(to, ndata);
        }
    } else if (width == 1) {
        uint8_t *p = to;
        for (int i = 0; i < ndata; i++) {
            //DEBUG
            //if (netDevDebug>0) {
            //    printf("%d %d 0x%08x 0x%08x\n", i, from[i], from[i], from[i]&0xff00);
            //}
#if 1
            if ((from[i] & 0xff00) /*&&
                (from[i] & 0xff00) != 0xff00 */) { // need check
                errlogPrintf("%s [%s:%d] illegal data conversion: width=%d ndata=%d i=%d from=%d\n", __func__, __FILE__, __LINE__, width, ndata, i, from[i]);
                return ERROR;
            }
#endif
            p[i] = from[i];
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    //DEBUG
    if (netDevDebug>0) {
        dump(from, sizeof(*from), to, width, ndata, __FILE__, __func__);
    }

    return OK;
}

//
// From Ulong record buffer
//
static long fromUlongVal(void *to,
                         uint32_t *from,
                         int noff,
                         int width,
                         int ndata,
                         int swap
                         )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    }

    from += noff;

    if (0) {
        //
    } else if (width == 4) {
        uint16_t *p = to;
        for (int i = 0; i < ndata; i++) {
            p[2*i  ] = from[i]>> 0;
            p[2*i+1] = from[i]>>16;
        }

        if (swap) {
            swap_bytes(to, 2*ndata);
        }
    } else if (width == 2) {
        uint16_t *p = to;
        for (int i = 0; i < ndata; i++) {
            //DEBUG
            //if (netDevDebug>0) {
            //    printf("%d %d 0x%08x 0x%08x\n", i, from[i], from[i], from[i]&0xffff0000);
            //}
#if 1
            if ((from[i] & 0xffff0000) /* &&
                (from[i] & 0xffff0000) != 0xffff0000 */) { // need check
                errlogPrintf("%s [%s:%d] illegal data conversion: width=%d ndata=%d i=%d from=%d\n", __func__, __FILE__, __LINE__, width, ndata, i, from[i]);
                return ERROR;
            }
#endif
            p[i] = from[i];
        }

        if (swap) {
            swap_bytes(to, ndata);
        }
    } else if (width == 1) {
        uint8_t *p = to;
        for (int i = 0; i < ndata; i++) {
            //DEBUG
            //if (netDevDebug>0) {
            //    printf("%d %d 0x%08x 0x%08x\n", i, from[i], from[i], from[i]&0xffffff00);
            //}
#if 1
            if ((from[i] & 0xffffff00) /* &&
                (from[i] & 0xffffff00) != 0xffffff00 */) { // need check
                errlogPrintf("%s [%s:%d] illegal data conversion: width=%d ndata=%d from=%d\n", __func__, __FILE__, __LINE__, width, ndata, *from);
                return ERROR;
            }
#endif
            p[i] = from[i];
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    //DEBUG
    if (netDevDebug>0) {
        dump(from, sizeof(*from), to, width, ndata, __FILE__, __func__);
    }

    return OK;
}

//
// From Float record buffer
//
static long fromFloatVal(void *to,
                         float *from,
                         int noff,
                         int width,
                         int ndata,
                         int swap
                         )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    }

    from += noff;

    if (0) {
        //
    } else if (width == 4) {
        uint16_t *p = to;
        for (int i = 0; i < ndata; i++) {
            uint32_t lval;
            memcpy(&lval, &from[i], sizeof(float));
            p[2*i  ] = lval>> 0;
            p[2*i+1] = lval>>16;
        }

        if (swap) {
            swap_bytes(to, 2*ndata);
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    //DEBUG
    if (netDevDebug>0) {
        dump(from, sizeof(*from), to, width, ndata, __FILE__, __func__);
    }

    return OK;
}

//
// From Double record buffer
//
static long fromDoubleVal(void *to,
                          double *from,
                          int noff,
                          int width,
                          int ndata,
                          int swap
                          )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %-13s (to=%8p noff=%d from=%8p width=%d ndata=%d swap=%d)\n", __FILE__, __func__, to, noff, from, width, ndata, swap);
    }

    from += noff;

    if (0) {
        //
    } else if (width == 8) {
        uint16_t *p = to;
        for (int i = 0; i < ndata; i++) {
            uint64_t lval;
            memcpy(&lval, &from[i], sizeof(double));
            p[4*i+0] = lval>> 0;
            p[4*i+1] = lval>>16;
            p[4*i+2] = lval>>32;
            p[4*i+3] = lval>>48;
        }

        if (swap) {
            swap_bytes(to, 4*ndata);
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    //DEBUG
    if (netDevDebug>0) {
        dump(from, sizeof(*from), to, width, ndata, __FILE__, __func__);
    }

    return OK;
}

//
// BCD to Integer conversion
//
uint32_t netDevBcd2Int(uint16_t bcd, void *precord)
{
    uint32_t base = 1;
    uint32_t dec  = 0;

    //if (bcd>BCDMAX_BCD) {
    //    recGblSetSevr(precord, HIGH_ALARM, INVALID_ALARM);
    //    return BCDMAX_INT;
    //}

    while (bcd > 0) {
        int digit = bcd & 0x000f;
        if (digit <= 9) {
            dec += digit * base;
        } else {
            // overflow
            recGblSetSevr((dbCommon *)precord, HIGH_ALARM, INVALID_ALARM); // cast required for base R3.14
            dec += 9 * base;
        }
        bcd >>= 4;
        base *= 10;
    }

    return dec;
}

//
// Integer to BCD conversion
//
uint16_t netDevInt2Bcd(int32_t dec, void *precord)
{
    if (dec<BCDMIN_INT) {
        recGblSetSevr((dbCommon *)precord, HW_LIMIT_ALARM, INVALID_ALARM); // cast required for base R3.14
        return BCDMIN_BCD;
    } else if (dec>BCDMAX_INT) {
        recGblSetSevr((dbCommon *)precord, HW_LIMIT_ALARM, INVALID_ALARM); // cast required for base R3.14
        return BCDMAX_BCD;
    }

    uint32_t base = 0;
    uint16_t bcd  = 0;

    while (dec > 0) {
        bcd |= ((dec%10) << base);
        dec /= 10;
        base += 4;
    }

    return bcd;
}

//
// Register symbols to be used by IOC core
//
epicsExportAddress(int, netDevDebug);

// end
