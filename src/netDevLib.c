/******************************************************************************
 *                         COPYRIGHT NOTIFICATION
 *
 * Copyright (c) All rights reserved
 *
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 ******************************************************************************/

#include <dbFldTypes.h>
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

    LOGMSG("devNetDev: parseLinkPlcCommon(%8p,%8p,%8p,%8p,%8p,%8p)\n",
           plink, peer_addr, route, unit, type, addr, 0, 0, 0);

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
    LOGMSG("devNetDev: buf[%ld]: %s\n", size, buf, 0, 0, 0, 0, 0, 0, 0);

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
            LOGMSG("devNetDev: pc0:%8p, pc1:%8p\n",pc0, pc1, 0, 0, 0, 0, 0, 0, 0);
            errlogPrintf("devNetDev: can't get port\n");
            return ERROR;
        }
        *pc1++ = '\0';

        //pc0 = NULL;
        pc0 = strchr(port, ':');
        if (pc0) {
            *pc0++ = '\0';
            *protocol = pc0;

            LOGMSG("devNetDev: protocol: %s\n",*protocol, 0, 0, 0, 0, 0, 0, 0, 0);
        }

        if (*port != '\0') { /* it was ':' */
            LOGMSG("devNetDev: string port: %s\n",port,0,0,0,0,0,0,0,0);

            unsigned int iport;
            if (strncmp(port, "0x", 2) == 0) {
                if (sscanf(port, "%x", &iport) != 1) {
                    errlogPrintf("devNetDev: can't get service port\n");
                    return ERROR;
                }
                LOGMSG("devNetDev: port in HEX: 0x%04x\n",iport,0,0,0,0,0,0,0,0);
            } else {
                if (sscanf(port, "%d", &iport) != 1) {
                    errlogPrintf("devNetDev: can't get service port\n");
                    return ERROR;
                }
                LOGMSG("devNetDev: port in DEC: %d\n",iport,0,0,0,0,0,0,0,0);
            }
            peer_addr->sin_port = htons(iport);
            LOGMSG("devNetDev: port: 0x%04x\n",ntohs(peer_addr->sin_port),0,0,0,0,0,0,0,0);
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
        LOGMSG("devNetDev: no device address specified\n",0,0,0,0,0,0,0,0,0);
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
        LOGMSG("devNetDev: no unit, no type\n",0,0,0,0,0,0,0,0,0);
    } else {
        *pc2++ = '\0';
        if (pc2 > pc1) {
            /* no unit, type exists ( hostname#type:addr ) */
            *unit = NULL;
            *type = pc1;
            *addr = pc2;
            LOGMSG("devNetDev: no unit, type exists\n",0,0,0,0,0,0,0,0,0);
        } else {
            /* unit exists. don't know about type. ( hostname:unit#[type:]addr ) */
            *unit = pc2;
            pc3 = strchr(pc1, ':');
            if (!pc3) {
                /* unit exists, but no type ( hostname:unit#addr ) */
                *type = NULL;
                *addr = pc1;
                LOGMSG("devNetDev: unit exists, but no type\n",0,0,0,0,0,0,0,0,0);
            } else {
                /* both unit and type exist ( hostname:unit#type:addr ) */
                *pc3++ = '\0';
                *type = pc1;
                *addr = pc3;
                LOGMSG("devNetDev: both unit and type exist\n",0,0,0,0,0,0,0,0,0);
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
    LOGMSG("devNetDev: hostid: 0x%08x\n",peer_addr->sin_addr.s_addr,0,0,0,0,0,0,0,0);

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
    LOGMSG("devNetDev: toRecordVal(%8p,%d,%d,%8p,%d,%d,%d)\n",
           bptr,noff,ftvl,buf,width,ndata,swap,0,0);

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
        errlogPrintf("toRecordVal: illeagal data conversion\n");
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
    LOGMSG("devNetDev: toCharVal(%8p,%d,%8p,%d,%d)\n",
           to,noff,from,width,ndata,0,0,0,0);

    to += noff;

    switch (width) {
    case 2:
    {
        int16_t *p = (int16_t *) from;

        for (int i=0; i < ndata; i++) {
            if (swap) {
                swap_bytes((uint16_t *)p);
            }

            if ((*p & 0xff80) &&
                (*p & 0xff80) != 0xff80) {
                errlogPrintf("toCharVal: illeagal data conversion\n");
                return ERROR;
            }

            *to++ = (int8_t) *p++;
      }
    }
    break;
    case 1:
    {
        int8_t *p = (int8_t *) from;

        for (int i=0; i < ndata; i++) {
            *to++ = (int8_t) *p++;
        }
    }
    break;
    default:
        errlogPrintf("toCharVal: illeagal data width\n");
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
    LOGMSG("devNetDev: toShortVal(%8p,%d,%8p,%d,%d)\n",
           to,noff,from,width,ndata,0,0,0,0);

    to += noff;

    switch (width) {
    case 2:
    {
        int16_t *p = (int16_t *) from;

        for (int i=0; i < ndata; i++) {
            if (swap) {
                swap_bytes((uint16_t *)p);
            }
            *to++ = (int16_t) *p++;
        }
    }
    break;
    case 1:
    {
        int8_t *p = (int8_t *) from;

        for (int i=0; i < ndata; i++) {
            *to++ = (int16_t) *p++;
        }
    }
    break;
    default:
        errlogPrintf("toShortVal: illeagal data width\n");
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
    LOGMSG("devNetDev: toLongVal(%8p,%d,%8p,%d,%d,%d)\n",
           to,noff,from,width,ndata,swap,0,0,0);

    to += noff;

    switch (width) {
    case 2:
    {
        int16_t *p = (int16_t *) from;

        for (int i=0; i < ndata; i++) {
            if (swap) {
                swap_bytes((uint16_t *)p);
            }
            *to++ = (int32_t) *p++;
        }
    }
    break;
    case 1:
    {
        int8_t *p = (int8_t *) from;

        for (int i=0; i < ndata; i++) {
            *to++ = (int32_t) *p++;
        }
    }
    break;
    default:
        errlogPrintf("toLongVal: illeagal data width\n");
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
    LOGMSG("devNetDev: toUcharVal(%8p,%d,%8p,%d,%d)\n",
           to,noff,from,width,ndata,0,0,0,0);

    to += noff;

    switch (width) {
    case 2:
    {
        uint16_t *p = (uint16_t *) from;

        for (int i=0; i < ndata; i++) {
            if (swap) {
                swap_bytes(p);
            }

            if (*p & 0xff00) {
                errlogPrintf("toUcharVal: illeagal data conversion\n");
                return ERROR;
            }

            *to++ = (uint8_t) *p++;
        }
    }
    break;
    case 1:
    {
        uint8_t *p = (uint8_t *) from;

        for (int i=0; i < ndata; i++) {
            *to++ = (uint8_t) *p++;
        }
    }
    break;
    default:
        errlogPrintf("toUcharVal: illeagal data width\n");
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
    LOGMSG("devNetDev: toUshortVal(%8p,%d,%8p,%d,%d)\n",
           to,noff,from,width,ndata,0,0,0,0);

    to += noff;

    switch (width) {
    case 2:
    {
        uint16_t *p = (uint16_t *) from;

        for (int i=0; i < ndata; i++) {
            if (swap) swap_bytes(p);
            *to++ = (uint16_t) *p++;
        }
    }
    break;
    case 1:
    {
        uint8_t *p = (uint8_t *) from;

        for (int i=0; i < ndata; i++) {
            *to++ = (uint16_t) *p++;
        }
    }
    break;
    default:
        errlogPrintf("toUshortVal: illeagal data width\n");
        return ERROR;
    }

    return OK;
}

/*******************************************************************************
 * To Ulong record buffer
 *******************************************************************************/
LOCAL long toUlongVal(
          uint32_t *to,
          int noff,
          void *from,
          int width,
          int ndata,
          int swap
          )
{
    LOGMSG("devNetDev: toUlongVal(%8p,%d,%8p,%d,%d,%d)\n",
           to,noff,from,width,ndata,swap,0,0,0);

    to += noff;

    switch (width)  {
    case 2:
    {
        uint16_t *p = (uint16_t *) from;

        for (int i=0; i < ndata; i++) {
            if (swap) {
                swap_bytes(p);
            }
            *to++ = (uint32_t) *p++;
        }
    }
    break;
    case 1:
    {
        uint8_t *p = (uint8_t *) from;

        for (int i=0; i < ndata; i++) {
            *to++ = (uint32_t) *p++;
        }
    }
    break;
    default:
        errlogPrintf("toUlongVal: illeagal data width\n");
        return ERROR;
    }

    return OK;
}

/*******************************************************************************
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
    LOGMSG("devNetDev: fromRecordVal(%8p,%d,%8p,%d,%d,%d,%d)\n",
           buf,width,bptr,noff,ftvl,ndata,swap,0,0);

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
        errlogPrintf("fromRecordVal: illeagal data conversion\n");
        return ERROR;
    }

    return OK;
}

/*******************************************************************************
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
    LOGMSG("devNetDev: fromCharVal(%8p,%8p,%d,%d,%d)\n",
           to,from,noff,width,ndata,0,0,0,0);

    from += noff;

    switch (width) {
    case 2:
    {
        int16_t *p = (int16_t *) to;

        for (int i=0; i < ndata; i++) {
            *p = (int16_t) *from++;
            if (swap) {
                swap_bytes((uint16_t *)p);
            }
            p++;
        }
    }
    break;
    case 1:
    {
        int8_t *p = (int8_t *) to;

        for (int i=0; i < ndata; i++) {
            *p++ = *from++;
        }
    }
    break;
    default:
        errlogPrintf("fromCharVal: illeagal data width\n");
        return ERROR;
    }

    return OK;
}

/*******************************************************************************
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
    LOGMSG("devNetDev: fromUshortVal(%8p,%8p,%d,%d,%d)\n",
           to,from,noff,width,ndata,0,0,0,0);

    from += noff;

    switch (width) {
    case 2:
    {
        int16_t *p = (int16_t *) to;

        for (int i=0; i < ndata; i++) {
            *p = (int16_t) *from++;
            if (swap) {
                swap_bytes((uint16_t *)p);
            }
            p++;
        }
    }
    break;
    case 1:
    {
        int8_t *p = (int8_t *) to;

        for (int i=0; i < ndata; i++) {
            if ((*from & 0xff80) &&
                (*from & 0xff80) != 0xff80) {
                errlogPrintf("fromShortVal: illeagal data conversion\n");
                return ERROR;
            }

            *p++ = (int8_t) *from++;
        }
    }
    break;
    default:
        errlogPrintf("fromShortVal: illeagal data width\n");
        return ERROR;
    }

    return OK;
}

/*******************************************************************************
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
    LOGMSG("netDevLib: fromLongVal(%8p,%8p,%d,%d,%d,%d)\n",
           to,from,noff,width,ndata,swap,0,0,0);

    from += noff;

    switch (width) {
    case 2:
    {
        int16_t *p = (int16_t *) to;

        for (int i=0; i < ndata; i++) {
            if ((*from & 0xffff8000) &&
                (*from & 0xffff8000) != 0xffff8000) {
                errlogPrintf("fromLongVal: illeagal data conversion\n");
                return ERROR;
            }

            *p = (int16_t) *from++;
            if (swap) swap_bytes((uint16_t *)p);
            p++;
        }
    }
    break;
    case 1:
    {
        int8_t *p = (int8_t *) to;

        for (int i=0; i < ndata; i++) {
            if ((*from & 0xffffff80) &&
                (*from & 0xffffff80) != 0xffffff80) {
                errlogPrintf("fromLongVal: illeagal data conversion\n");
                return ERROR;
            }

            *p++ = (int8_t) *from++;
        }
    }
    break;
    default:
        errlogPrintf("fromLongVal: illeagal data width\n");
        return ERROR;
    }

    return OK;
}

/*******************************************************************************
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
    LOGMSG("devNetDev: fromUcharVal(%8p,%8p,%d,%d,%d)\n",
           to,from,noff,width,ndata,0,0,0,0);

    from += noff;

    switch (width) {
    case 2:
    {
        uint16_t *p = (uint16_t *) to;

        for (int i=0; i < ndata; i++) {
            *p = (uint16_t) *from++;
            if (swap) {
                swap_bytes(p);
            }
            p++;
        }
    }
    break;
    case 1:
    {
        uint8_t *p = (uint8_t *) to;

        for (int i=0; i < ndata; i++) {
            *p++ = *from++;
        }
    }
    break;
    default:
        errlogPrintf("fromUcharVal: illeagal data width\n");
        return ERROR;
    }

    return OK;
}

/*******************************************************************************
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
    LOGMSG("devNetDev: fromUshortVal(%8p,%8p,%d,%d,%d)\n",
           to,from,noff,width,ndata,0,0,0,0);

    from += noff;

    switch (width) {
    case 2:
    {
        uint16_t *p = (uint16_t *) to;

        for (int i=0; i < ndata; i++) {
            *p = (uint16_t) *from++;
            if (swap) {
                swap_bytes(p);
            }
            p++;
        }
    }
    break;
    case 1:
    {
        uint8_t *p = (uint8_t *) to;

        for (int i=0; i < ndata; i++) {
            if (*from & 0xff00) {
                errlogPrintf("fromUshortVal: illeagal data conversion\n");
                return ERROR;
            }

            *p++ = (uint8_t) *from++;
        }
    }
    break;
    default:
        errlogPrintf("fromUshortVal: illeagal data width\n");
        return ERROR;
    }

    return OK;
}

/*******************************************************************************
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
    LOGMSG("devNetDev: fromUlongVal(%8p,%8p,%d,%d,%d,%d)\n",
           to,from,noff,width,ndata,swap,0,0,0);

    from += noff;

    switch (width) {
    case 2:
    {
        uint16_t *p = (uint16_t *) to;

        for (int i=0; i < ndata; i++) {
            if (*from & 0xffff0000) {
                errlogPrintf("fromUlongVal: illeagal data conversion\n");
                return ERROR;
            }

            *p = (uint16_t) *from++;
            if (swap) {
                swap_bytes(p);
            }
            p++;
        }
    }
    break;
    case 1:
    {
        uint8_t *p = (uint8_t *) to;

        for (int i=0; i < ndata; i++) {
            if (*from & 0xffffff00) {
                errlogPrintf("fromUlongVal: illeagal data conversion\n");
                return ERROR;
            }

            *p++ = (uint8_t) *from++;
        }
    }
    break;
    default:
        errlogPrintf("fromUlongVal: illeagal data width\n");
        return ERROR;
    }

    return OK;
}

/*******************************************************************************
 *
 ******************************************************************************/
void swap_bytes(uint16_t *p)
{
    char *pC = (char *) p;
    char tmp = *pC;

    *pC = *(pC + 1);
    *(pC + 1) = tmp;
}
