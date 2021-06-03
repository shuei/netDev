/******************************************************************************
 *                         COPYRIGHT NOTIFICATION
 *
 * Copyright (c) All rights reserved
 *
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 ******************************************************************************/

//
#include <dbFldTypes.h>
#include <alarm.h>
#include <recGbl.h>

//
#include "drvNetMpf.h"
#include "devNetDev.h"

/******************************************************************************
 * Routines for signed integer data transfer
 ******************************************************************************/
LOCAL long toCharVal   (int8_t  *, int, void *, int, int, int);
LOCAL long toShortVal  (int16_t *, int, void *, int, int, int);
LOCAL long toLongVal   (int32_t *, int, void *, int, int, int);
LOCAL long fromCharVal (void *, int8_t *,  int, int, int, int);
LOCAL long fromShortVal(void *, int16_t *, int, int, int, int);
LOCAL long fromLongVal (void *, int32_t *, int, int, int, int);

/******************************************************************************
 * Routines for unsigned integer data transfer
 ******************************************************************************/
LOCAL long toUcharVal   (uint8_t  *, int, void *, int, int, int);
LOCAL long toUshortVal  (uint16_t *, int, void *, int, int, int);
LOCAL long toUlongVal   (uint32_t *, int, void *, int, int, int);
LOCAL long fromUcharVal (void *, uint8_t  *, int, int, int, int);
LOCAL long fromUshortVal(void *, uint16_t *, int, int, int, int);
LOCAL long fromUlongVal (void *, uint32_t *, int, int, int, int);

/******************************************************************************
 *
 ******************************************************************************/
#define BCDMAX_BCD 39321 // 0x9999
#define BCDMAX_INT  9999 // 0x9999

#define BCDMIN_BCD     0 // 0x9999
#define BCDMIN_INT     0 //

/******************************************************************************
 *
 ******************************************************************************/
void swap_bytes(uint16_t *);

/******************************************************************************
 * Link field parser for PLCs
 ******************************************************************************/
long parseLinkPlcCommon(struct link *plink,
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

    /*
     * hostname[{routing_path}][:unit]#[type:]addr[&option]
     *
     * hostname[:unit]#[type:]addr
     */

    size_t size = strlen(plink->value.instio.string) + 1;
    if (size > MAX_INSTIO_STRING) {
        errlogPrintf("devNetDev: instio.string is too long\n");
        return ERROR;
    }

    char *buf = (char *) calloc(1, size);
    if (!buf) {
        errlogPrintf("devNetDev: can't calloc for buf\n");
        return ERROR;
    }

    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';
    LOGMSG("devNetDev: buf[%zd]: %s\n", size, buf);

    pc0 = strchr(buf, '&');
    if (pc0) {
        *pc0++ = '\0';
        *lopt = pc0;
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

        if (*port != '\0') { /* it was ':' */
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
        /* no unit, no type ( hostname#addr ) */
        *unit = NULL;
        *type = NULL;
        *addr = pc1;
        LOGMSG("devNetDev: no unit, no type\n");
    } else {
        *pc2++ = '\0';
        if (pc2 > pc1) {
            /* no unit, type exists ( hostname#type:addr ) */
            *unit = NULL;
            *type = pc1;
            *addr = pc2;
            LOGMSG("devNetDev: no unit, type exists\n");
        } else {
            /* unit exists. don't know about type. ( hostname:unit#[type:]addr ) */
            *unit = pc2;
            pc3 = strchr(pc1, ':');
            if (!pc3) {
                /* unit exists, but no type ( hostname:unit#addr ) */
                *type = NULL;
                *addr = pc1;
                LOGMSG("devNetDev: unit exists, but no type\n");
            } else {
                /* both unit and type exist ( hostname:unit#type:addr ) */
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

/******************************************************************************
 * Copy values from receive buffer to record
 ******************************************************************************/
long toRecordVal(void *bptr,
                 int   noff,
                 int   ftvl,
                 void *buf,
                 int   width,
                 int   ndata,
                 int   swap
                 )
{
    LOGMSG("devNetDev: %s(%8p,%d,%d,%8p,%d,%d,%d)\n", __func__, bptr, noff, ftvl, buf, width, ndata, swap);

    switch (ftvl) {
    case DBF_CHAR:
        return toCharVal(bptr, noff, buf, width, ndata, swap);
    case DBF_SHORT:
        return toShortVal(bptr, noff, buf, width, ndata, swap);
    case DBF_LONG:
        return toLongVal(bptr, noff, buf, width, ndata, swap);
    case DBF_UCHAR:
        return toUcharVal(bptr, noff, buf, width, ndata, swap);
    case DBF_USHORT:
        return toUshortVal(bptr, noff, buf, width, ndata, swap);
    case DBF_ULONG:
        return toUlongVal(bptr, noff, buf, width, ndata, swap);
    default:
        errlogPrintf("devNetDev: %s: unsupported FTVL: %d\n", __func__, ftvl);
        return ERROR;
    }

    return OK;
}

/******************************************************************************
 * To Char record buffer
 ******************************************************************************/
LOCAL long toCharVal(int8_t *to,
                     int noff,
                     void *from,
                     int width,
                     int ndata,
                     int swap
                     )
{
    LOGMSG("devNetDev: %s(%8p,%d,%8p,%d,%d)\n", __func__, to, noff, from, width, ndata);

    to += noff;

    if (width == 2) {
        int16_t *p = (int16_t *) from;

        for (int i=0; i < ndata; i++) {
            if (swap) {
                swap_bytes((uint16_t *)p);
            }

            if ((*p & 0xff80) &&
                (*p & 0xff80) != 0xff80) {
                errlogPrintf("%s [%s:%d] illegal data conversion: width=%d ndata=%d from=%d\n", __func__, __FILE__, __LINE__, width, ndata, *p);
                return ERROR;
            }

            *to++ = (int8_t) *p++;
        }
    } else if (width == 1) {
        int8_t *p = (int8_t *) from;

        for (int i=0; i < ndata; i++) {
            *to++ = (int8_t) *p++;
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    return OK;
}

/******************************************************************************
 * To Short record buffer
 ******************************************************************************/
LOCAL long toShortVal(int16_t *to,
                      int noff,
                      void *from,
                      int width,
                      int ndata,
                      int swap
                      )
{
    LOGMSG("devNetDev: %s(%8p,%d,%8p,%d,%d)\n", __func__, to, noff, from, width, ndata);

    to += noff;

    if (width==2) {
        int16_t *p = (int16_t *) from;
        for (int i=0; i < ndata; i++) {
            if (swap) {
                swap_bytes((uint16_t *)p);
            }
            *to++ = (int16_t) *p++;
        }
    } else if (width == 1) {
        int8_t *p = (int8_t *) from;

        for (int i=0; i < ndata; i++) {
            *to++ = (int16_t) *p++;
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    return OK;
}

/******************************************************************************
 * To Long record buffer
 ******************************************************************************/
LOCAL long toLongVal(int32_t *to,
                     int noff,
                     void *from,
                     int width,
                     int ndata,
                     int swap
                     )
{
    LOGMSG("devNetDev: %s(%8p,%d,%8p,%d,%d)\n", __func__, to, noff, from, width, ndata);

    to += noff;

    if (width == 2) {
        int16_t *p = (int16_t *) from;

        for (int i=0; i < ndata; i++) {
            if (swap) {
                swap_bytes((uint16_t *)p);
            }
            *to++ = (int32_t) *p++;
        }
    } else if (width == 1) {
        int8_t *p = (int8_t *) from;

        for (int i=0; i < ndata; i++) {
            *to++ = (int32_t) *p++;
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    return OK;
}

/******************************************************************************
 * To Uchar record buffer
 ******************************************************************************/
LOCAL long toUcharVal(uint8_t *to,
                      int noff,
                      void *from,
                      int width,
                      int ndata,
                      int swap
                      )
{
    LOGMSG("devNetDev: %s(%8p,%d,%8p,%d,%d)\n", __func__, to, noff, from, width, ndata);

    to += noff;

    if (width == 2) {
        uint16_t *p = (uint16_t *) from;

        for (int i=0; i < ndata; i++) {
            if (swap) {
                swap_bytes(p);
            }

            if (*p & 0xff00) {
                errlogPrintf("%s [%s:%d] illegal data conversion: width=%d ndata=%d from=%d\n", __func__, __FILE__, __LINE__, width, ndata, *p);
                return ERROR;
            }

            *to++ = (uint8_t) *p++;
        }
    } else if (width == 1) {
        uint8_t *p = (uint8_t *) from;

        for (int i=0; i < ndata; i++) {
            *to++ = (uint8_t) *p++;
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    return OK;
}

/******************************************************************************
 * To Ushort record buffer
 ******************************************************************************/
LOCAL long toUshortVal(uint16_t *to,
                       int noff,
                       void *from,
                       int width,
                       int ndata,
                       int swap
                       )
{
    LOGMSG("devNetDev: %s(%8p,%d,%8p,%d,%d)\n", __func__, to, noff, from, width, ndata);

    to += noff;

    if (width == 2) {
        uint16_t *p = (uint16_t *) from;

        for (int i=0; i < ndata; i++) {
            if (swap) swap_bytes(p);
            *to++ = (uint16_t) *p++;
        }
    } else if (width == 1) {
        uint8_t *p = (uint8_t *) from;

        for (int i=0; i < ndata; i++) {
            *to++ = (uint16_t) *p++;
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    return OK;
}

/******************************************************************************
 * To Ulong record buffer
 ******************************************************************************/
LOCAL long toUlongVal(
          uint32_t *to,
          int noff,
          void *from,
          int width,
          int ndata,
          int swap
          )
{
    LOGMSG("devNetDev: %s(%8p,%d,%8p,%d,%d)\n", __func__, to, noff, from, width, ndata);

    to += noff;

    if (width == 2)  {
        uint16_t *p = (uint16_t *) from;

        for (int i=0; i < ndata; i++) {
            if (swap) {
                swap_bytes(p);
            }
            *to++ = (uint32_t) *p++;
        }
    } else if (width == 1) {
        uint8_t *p = (uint8_t *) from;

        for (int i=0; i < ndata; i++) {
            *to++ = (uint32_t) *p++;
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    return OK;
}

/******************************************************************************
 * Copy values from record to send buffer
 ******************************************************************************/
long fromRecordVal(void *buf,
                   int   width,
                   void *bptr,
                   int   noff,
                   int   ftvl,
                   int   ndata,
                   int   swap
                   )
{
    LOGMSG("devNetDev: %s(%8p,%d,%8p,%d,%d,%d,%d)\n", __func__, buf, width, bptr, noff, ftvl, ndata, swap);

    switch (ftvl) {
    case DBF_CHAR:
        return fromCharVal(buf, bptr, noff, width, ndata, swap);
    case DBF_SHORT:
        return fromShortVal(buf, bptr, noff, width, ndata, swap);
    case DBF_LONG:
        return fromLongVal(buf, bptr, noff, width, ndata, swap);
    case DBF_UCHAR:
        return fromUcharVal(buf, bptr, noff, width, ndata, swap);
    case DBF_USHORT:
        return fromUshortVal(buf, bptr, noff, width, ndata, swap);
    case DBF_ULONG:
        return fromUlongVal(buf, bptr, noff, width, ndata, swap);
    default:
        errlogPrintf("devNetDev: %s: unsupported FTVL: %d\n", __func__, ftvl);
        return ERROR;
    }

    return OK;
}

/******************************************************************************
 * From Char record buffer
 ******************************************************************************/
LOCAL long fromCharVal(void *to,
                       int8_t *from,
                       int noff,
                       int width,
                       int ndata,
                       int swap
                       )
{
    LOGMSG("devNetDev: %s(%8p,%d,%8p,%d,%d)\n", __func__, to, noff, from, width, ndata);

    from += noff;

    if (width == 2) {
        int16_t *p = (int16_t *) to;

        for (int i=0; i < ndata; i++) {
            *p = (int16_t) *from++;
            if (swap) {
                swap_bytes((uint16_t *)p);
            }
            p++;
        }
    } else if (width == 1) {
        int8_t *p = (int8_t *) to;

        for (int i=0; i < ndata; i++) {
            *p++ = *from++;
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    return OK;
}

/******************************************************************************
 * From Short record buffer
 ******************************************************************************/
LOCAL long fromShortVal(void *to,
                        int16_t *from,
                        int noff,
                        int width,
                        int ndata,
                        int swap
                        )
{
    LOGMSG("devNetDev: %s(%8p,%d,%8p,%d,%d)\n", __func__, to, noff, from, width, ndata);

    from += noff;

    if (width == 2) {
        int16_t *p = (int16_t *) to;

        for (int i=0; i < ndata; i++) {
            *p = (int16_t) *from++;
            if (swap) {
                swap_bytes((uint16_t *)p);
            }
            p++;
        }
    } else if (width == 1) {
        int8_t *p = (int8_t *) to;

        for (int i=0; i < ndata; i++) {
            if ((*from & 0xff80) &&
                (*from & 0xff80) != 0xff80) {
                errlogPrintf("%s [%s:%d] illegal data conversion: width=%d ndata=%d from=%d\n", __func__, __FILE__, __LINE__, width, ndata, *from);
                return ERROR;
            }

            *p++ = (int8_t) *from++;
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    return OK;
}

/******************************************************************************
 * From Long record buffer
 ******************************************************************************/
LOCAL long fromLongVal(void *to,
                       int32_t *from,
                       int noff,
                       int width,
                       int ndata,
                       int swap
                       )
{
    LOGMSG("devNetDev: %s(%8p,%d,%8p,%d,%d)\n", __func__, to, noff, from, width, ndata);

    from += noff;

    if (width == 2) {
        int16_t *p = (int16_t *) to;

        for (int i=0; i < ndata; i++) {
            if ((*from & 0xffff8000) &&
                (*from & 0xffff8000) != 0xffff8000) {
                errlogPrintf("%s [%s:%d] illegal data conversion: width=%d ndata=%d from=%d\n", __func__, __FILE__, __LINE__, width, ndata, *from);
                return ERROR;
            }

            *p = (int16_t) *from++;
            if (swap) swap_bytes((uint16_t *)p);
            p++;
        }
    } else if (width == 1) {
        int8_t *p = (int8_t *) to;

        for (int i=0; i < ndata; i++) {
            if ((*from & 0xffffff80) &&
                (*from & 0xffffff80) != 0xffffff80) {
                errlogPrintf("%s [%s:%d] illegal data conversion: width=%d ndata=%d from=%d\n", __func__, __FILE__, __LINE__, width, ndata, *from);
                return ERROR;
            }

            *p++ = (int8_t) *from++;
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    return OK;
}

/******************************************************************************
 * From Uchar record buffer
 ******************************************************************************/
LOCAL long fromUcharVal(void *to,
                        uint8_t *from,
                        int noff,
                        int width,
                        int ndata,
                        int swap
                        )
{
    LOGMSG("devNetDev: %s(%8p,%d,%8p,%d,%d)\n", __func__, to, noff, from, width, ndata);

    from += noff;

    if (width == 2) {
        uint16_t *p = (uint16_t *) to;

        for (int i=0; i < ndata; i++) {
            *p = (uint16_t) *from++;
            if (swap) {
                swap_bytes(p);
            }
            p++;
        }
    } else if (width == 1) {
        uint8_t *p = (uint8_t *) to;

        for (int i=0; i < ndata; i++) {
            *p++ = *from++;
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    return OK;
}

/******************************************************************************
 * From Ushort record buffer
 ******************************************************************************/
LOCAL long fromUshortVal(void *to,
                         uint16_t *from,
                         int noff,
                         int width,
                         int ndata,
                         int swap
                         )
{
    LOGMSG("devNetDev: %s(%8p,%d,%8p,%d,%d)\n", __func__, to, noff, from, width, ndata);

    from += noff;

    if (width == 2) {
        uint16_t *p = (uint16_t *) to;

        for (int i=0; i < ndata; i++) {
            *p = (uint16_t) *from++;
            if (swap) {
                swap_bytes(p);
            }
            p++;
        }
    } else if (width == 1) {
        uint8_t *p = (uint8_t *) to;

        for (int i=0; i < ndata; i++) {
            if (*from & 0xff00) {
                errlogPrintf("%s [%s:%d] illegal data conversion: width=%d ndata=%d from=%d\n", __func__, __FILE__, __LINE__, width, ndata, *from);
                return ERROR;
            }

            *p++ = (uint8_t) *from++;
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    return OK;
}

/******************************************************************************
 * From Ulong record buffer
 ******************************************************************************/
LOCAL long fromUlongVal(void *to,
                        uint32_t *from,
                        int noff,
                        int width,
                        int ndata,
                        int swap
                        )
{
    LOGMSG("devNetDev: %s(%8p,%d,%8p,%d,%d)\n", __func__, to, noff, from, width, ndata);

    from += noff;

    if (width == 2) {
        uint16_t *p = (uint16_t *) to;

        for (int i=0; i < ndata; i++) {
            if (*from & 0xffff0000) {
                errlogPrintf("%s [%s:%d] illegal data conversion: width=%d ndata=%d from=%d\n", __func__, __FILE__, __LINE__, width, ndata, *from);
                return ERROR;
            }

            *p = (uint16_t) *from++;
            if (swap) {
                swap_bytes(p);
            }
            p++;
        }
    } else if (width == 1) {
        uint8_t *p = (uint8_t *) to;

        for (int i=0; i < ndata; i++) {
            if (*from & 0xffffff00) {
                errlogPrintf("%s [%s:%d] illegal data conversion: width=%d ndata=%d from=%d\n", __func__, __FILE__, __LINE__, width, ndata, *from);
                return ERROR;
            }

            *p++ = (uint8_t) *from++;
        }
    } else {
        errlogPrintf("%s [%s:%d] illegal data width: %d\n", __func__, __FILE__, __LINE__, width);
        return ERROR;
    }

    return OK;
}

/******************************************************************************
 * BCD to Integer conversion
 ******************************************************************************/
uint32_t netDevBcd2Int(uint16_t bcd, void *precord)
{
    uint32_t base = 1;
    uint32_t dec  = 0;

    //if (bcd>BCDMAX_BCD) {
    //    recGblSetSevr(precord, HIGH_ALARM, INVALID_ALARM);
    //    return BCDMAX_INT;
    //}

    while (bcd>0) {
        int digit = bcd & 0x000f;
        if (digit <= 9) {
            dec += digit * base;
        } else {
            // overflow
            recGblSetSevr(precord, HIGH_ALARM, INVALID_ALARM);
            dec += 9 * base;
        }
        bcd >>= 4;
        base *= 10;
    }

    return dec;
}

/******************************************************************************
 * Integer to BCD conversion
 ******************************************************************************/
uint16_t netDevInt2Bcd(int32_t dec, void *precord)
{
    if (dec<BCDMIN_INT) {
        recGblSetSevr(precord, HW_LIMIT_ALARM, INVALID_ALARM);
        return BCDMIN_BCD;
    } else if (dec>BCDMAX_INT) {
        recGblSetSevr(precord, HW_LIMIT_ALARM, INVALID_ALARM);
        return BCDMAX_BCD;
    }

    uint32_t base = 0;
    uint16_t bcd  = 0;

    while (dec>0) {
        bcd |= ((dec%10) << base);
        dec /= 10;
        base += 4;
    }

    return bcd;
}

/******************************************************************************
 *
 ******************************************************************************/
void swap_bytes(uint16_t *p)
{
    char *pC = (char *) p;
    char tmp = *pC;

    *pC = *(pC + 1);
    *(pC + 1) = tmp;
}
