/* devChinoLogL.c */
/****************************************************************************
 *                         COPYRIGHT NOTIFICATION
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ****************************************************************************/
/* Author : Makoto Tobiyama 19/Jul/2005 */

#include <dbFldTypes.h>
#include <epicsExport.h>

#include "drvNetMpf.h"
#include "devNetDev.h"

#define CHINOL_CMND_LENGTH 8  // not correct
#define CHINOL_UDP_PORT    11111
#define CHINOL_GET_PROTO chino_get_protocol()
#define CHINOL_MAX_NDATA 5120
#define CHINOL_DATA_OFFSET 3

//static int finsUseSamePortNumber = MPF_SAMEPORT;
static unsigned short calCRC(unsigned char cd[], int nmax);
static int TwoRawToVal(unsigned char bu[],int offset, float *res);
static long chino_parse_link(DBLINK *, struct sockaddr_in *, int *, void *);
static int chino_get_protocol(void);
static void *chino_calloc(int, int, int, int, int);

typedef struct {
    int      unit;
    int      type;
    int      chan;
    int      bit;
    int      width;
    int      slvA;
    int      cmd;
    int      start;
    int      ndata;
    int      data_trans;
    char    *lopt;
} CHINOL_LOG;

static long chino_config_command(uint8_t *, int *, void *, int, int, int *, CHINOL_LOG *, int);
static long chino_parse_response(uint8_t *, int *, void *, int, int, int *, CHINOL_LOG *, int);

int chinoLogLUseTcp = 1; // we'd better make this static until we are able to change this via iocsh command line.

static int chino_get_protocol(void)
{
    if (chinoLogLUseTcp) {
        return MPF_TCP;
    }

    return MPF_UDP;
}

static void *chino_calloc(int unit, int type, int chan, int bit, int width)
{
    CHINOL_LOG *d = calloc(1, sizeof(CHINOL_LOG));
    if (!d) {
        errlogPrintf("devChinoLogL: calloc failed\n");
        return NULL;
    }

    d->unit   = unit;
    d->type   = type;
    d->chan   = chan;
    d->bit    = bit;
    d->width  = width;

    return d;
}

#include "devWaveformChinoLogL.c"

//
// Link field parser for command/response I/O
//
static long chino_parse_link(DBLINK *plink,
                             struct sockaddr_in *peer_addr,
                             int *option,
                             void *device
                             )
{
    char *protocol = NULL;
    char *unit  = NULL;
    char *type  = NULL;
    char *addr  = NULL;
    char *route = NULL;
    char *lopt  = NULL;
    CHINOL_LOG *d = (CHINOL_LOG *)device;

    if (parseLinkPlcCommon(plink,
                           peer_addr,
                           &protocol,
                           &route, // dummy
                           &unit,
                           &type,
                           &addr,
                           &lopt
                           )) {
        errlogPrintf("devChinoLogL: illeagal input specification\n");
        return ERROR;
    }

    LOGMSG("chino parse link\n");

    if (unit) {
        if (sscanf(unit,"%x",&d->slvA) != 1) {
            errlogPrintf("devChinoLogL :cannot get slave address\n");
            return ERROR;
        }
        LOGMSG("devChinoLogL: slave address: %d\n", d->slvA);
    } else {
        errlogPrintf("devChinoLogL: no slave address specified\n");
        return ERROR;
    }

    if (type) {
        if (sscanf(type,"%x",&d->cmd) != 1) {
            errlogPrintf("devChinoLogL :cannot get command\n");
            return ERROR;
        }
        LOGMSG("devChinoLogL: command\n", d->cmd);
    } else {
        errlogPrintf("devChinoLogL: no command specified\n");
        return ERROR;
    }

    if (addr) {
        if (sscanf(addr,"%d",&d->start) != 1) {
            errlogPrintf("devChinoLogL : cannot get start address\n");
            return ERROR;
        }
    } else {
        errlogPrintf("devChinoLogL: no start address specified\n");
        return ERROR;
    }

    if (lopt) {
        if (sscanf(lopt,"%d",&d->data_trans) != 1) {
            errlogPrintf("devChinoLogL : cannot get transfer length\n");
            return ERROR;
        }
    } else {
        //errlogPrintf("devChinoLogL: no transfer length specified\n");
        //return ERROR;
        d->data_trans = 12;
        LOGMSG("devChinoLogL: no transfer length specified. Assuming %d\n", d->data_trans);
    }

    LOGMSG("parse start %d trans %d\n",d->start, d->data_trans);
    return OK;
}

//
// Command constructor for command/response I/O
//
static unsigned short calCRC(unsigned char cd[], int nmax)
{
    unsigned short iP  = 0x0000;

    unsigned short iCRC = 0xffff;

    for (int i=0; i<nmax; i++) {
        iCRC = iCRC ^ cd[i];
        for (int j=1; j<=8; j++) {
            unsigned short iCY = iCRC & 0x1;\

            if ((iCRC & 0x8000) == 0x8000) {
                iP = 0x4000;
                iCRC = iCRC & 0x7fff;
            } else {
                iP = 0x0;
            }

            iCRC = iCRC>>1;
            iCRC = iCRC | iP;
            if (iCY == 1) {
                iCRC = iCRC ^ 0xa001;
            }
        }
    }

    if ((iCRC & 0x8000) == 0x8000) {
        iP = 0x80;
        iCRC = iCRC & 0x7fff;
    } else {
        iP = 0;
    }

    unsigned char iC1 = iCRC & 0xff;
    unsigned char iC2 = ((iCRC & 0x7f00) >>8) | iP;

    return iC1*256 + iC2;
}

static long chino_config_command(uint8_t *buf,    // driver buf addr
                                 int     *len,    // driver buf size
                                 void    *bptr,   // record buf addr
                                 int      ftvl,   // record field type
                                 int      ndata,  // n to be transferred
                                 int     *option, // direction etc.
                                 CHINOL_LOG *d,
                                 int      sid
                                 )
{
    LOGMSG("devChinoLogL: chino_config_command(0x%08x,%d,0x%08x,%d,%d,%d,0x%08x)\n",
           buf,*len,bptr,ftvl,ndata,*option,d,0,0);

    int n = ndata;
    int nwrite = isWrite(*option) ? (d->width)*n : 0;

    if (*len < CHINOL_CMND_LENGTH + nwrite) {
        errlogPrintf("devChinoLogL: buffer is running short\n");
        return ERROR;
    }

    buf[ 0] = d->slvA;           // slave address
    buf[ 1] = d->cmd;            // command
    buf[ 2] = d->start>>8;       // start address (H)
    buf[ 3] = d->start;          // start address (L)
    buf[ 4] = 0x00;              // number (H)
    buf[ 5] = (d->data_trans)*2; // number (L) 0x0c

    int resCRC = calCRC(buf,6);

    buf[ 6] = resCRC>>8;          // crc (H)
    buf[ 7] = resCRC;             // crc (L)

    LOGMSG("devChinoLogL: command = %x %x %x %x %x %x %x %x\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7]);

    *len = CHINOL_CMND_LENGTH;

#if 0
    if (isWrite(*option)) {
        if (fromRecordVal(
                          &buf[CHINOL_CMND_LENGTH],
                          d->width,
                          bptr,
                          d->noff,
                          ftvl,
                          n,
                          CHINOL_NEEDS_SWAP
                          )) {
            errlogPrintf("devChinoLogL: illeagal command\n");
            return ERROR;
        }
    }
#endif

#if 0
    nread  = isRead(*option)? (d->width)*n:0;
    return (CHINOL_DATA_OFFSET + nread);
#endif
    return 0;
}

//
// Response parser for command/response I/O
//
static int TwoRawToVal(unsigned char bu[], int offset, float *res)
{
    float f1;

    if ((bu[offset+2] & 0x20) == 0x20) {
        f1 = bu[offset]*256 + bu[offset+1];
    }
    else if ((bu[offset] & 0x80) == 0x80) {
        f1 = (bu[offset]-255)*256 + (bu[offset+1]-255);
    }
    else {
        f1 = bu[offset]*256 + bu[offset+1];
    }

    switch (bu[offset+3] & 0x0f) {
    case 0:
        *res = f1;
        break;
    case 1:
        *res = f1*0.1;
        break;
    case 2:
        *res = f1*0.01;
        break;
    case 3:
        *res = f1*0.001;
        break;
    case 4:
        *res = f1*0.0001;
        break;
    default:
        return -1; // this value is passed to chino_parse_response() but will be ignored
    }

    return (bu[offset+3] & 0xf0);
}

static long chino_parse_response(uint8_t *buf,    // driver buf addr
                                 int     *len,    // driver buf size
                                 void    *bptr,   // record buf addr
                                 int      ftvl,   // record field type
                                 int      ndata,  // n to be transferred
                                 int     *option, // direction etc.
                                 CHINOL_LOG *d,
                                 int      sid
                                 )
{
    LOGMSG("devChinoLogL: chino_parse_response(0x%08x,%d,0x%08x,%d,%d,%d,0x%08x)\n",
           buf,len,bptr,ftvl,ndata,*option,d,0,0);

#if 0
    for (int i=0;i<12;i++) {
        errlogPrintf("devChinoLogL buffer %d = %x %x %x %x\n",i*4,buf[CHINOL_DATA_OFFSET+i*4],buf[CHINOL_DATA_OFFSET+i*4+1],buf[CHINOL_DATA_OFFSET+i*4+2],buf[CHINOL_DATA_OFFSET+i*4+3]);
    }
#endif

    float temp[1000];
    int ret = 0;
    if (isRead(*option)) {
        for (int i=0; i<(d->data_trans); i++) {
            ret = TwoRawToVal(buf, CHINOL_DATA_OFFSET+i*4, &temp[i]);
            if (ret != 0) {
                switch (ret & 0xf0) {
                case 0x10:
                    temp[i] = -10.0;
                    break;
                case 0x20:
                    temp[i] = 10.0;
                    break;
                case 0x30:
                    temp[i] = -1000.0;
                    break;
                default:
                    temp[i] = -1000.0; // we'd better to change this value to someting else, e.g. -9999.
                    LOGMSG("devChinoLog Emergency code for i=%d ret=0x%x\n",i,ret);
                }
            }
        }
    }

    ndata = d->data_trans;
    float *ptemp = temp;
    float *rawVal = bptr;
    int i = ndata;

    while (i--) {
        *rawVal++ = *ptemp++;
    }

    ret = 0; // returns always zero
    return ret;
}
