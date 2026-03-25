/* devYewPlc.c */
/****************************************************************************
 *                         COPYRIGHT NOTIFICATION
 *
 * Copyright (c) All rights reserved
 *
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 ****************************************************************************/
/* Author: Jun-ichi Odagiri (jun-ichi.odagiri@kek.jp, KEK) */
/* Modification Log:
 * -----------------
 * 2005/08/22 jio
 *  passing option flag to Link Field Parser was changed from by value to
 * by pointer
 * 2006/01/12 jio
 *  added support to access registers of "Special Modules"
 *  enabled to select TCP/UDP dinamically
 * 2006/02/22 jio: added support for long word and floating point data,
 * made all integer data signed
 */

#include <ctype.h>

#include <dbFldTypes.h>
#include <epicsExport.h>

#include "drvNetMpf.h"
#include "devNetDev.h"

//
extern int netDevDebug;

//
#define YEW_CMND_LENGTH(x)       ((x) ? 10 : 12)
#define YEW_DATA_OFFSET          4
#define YEW_DEFAULT_CPU          1
#define YEW_DEFAULT_MODULE_UNIT  0
#define YEW_PROTOCOL             (yewProtocol)
#define YEW_MAX_BYTES            (yewMaxBytes)
#define YEW_NEEDS_SWAP           (__BYTE_ORDER==__LITTLE_ENDIAN)
#define YEW_SPECIAL_MODULE

static int    yewProtocol     = MPF_UDP; // MPF_TCP
static int    yewPort         = 0x3001;  // 0x3001(12289):port-A, 0x3003(12291):port-B
static int    yewMaxBytes     = 64*2;    // Maximum transfer bytes allowed

static double yewSendTimeout  = 0;
static double yewRecvTimeout  = DEFAULT_TIMEOUT;
static double yewEpicsTimerTimeout = 0;

//
typedef enum {
    kBit   = 1, // Bit device
    kWord  = 2, // Word device
    kDWord = 4, // Word device w/ Long-Word (2 words) access
    kQWord = 8, // Word device w/ Double-Long-Word (4 words) access
//    kXWord = 4, // Word device w/ Long-Word (2 words) access (F3XP01 / 02)
} width_t;

//
typedef enum {
    kSHORT  = 0,  //
    kUSHORT = 1,  //
    kLONG   = 2,  //
    kULONG  = 3,  //
    kFLOAT  = 4,  //
    kDOUBLE = 5,  //
    kBCD    = 6,  //
} conv_t;

static char *convstr[] = {
    "<none>",
    "U",
    "L",
    "UL",
    "F",
    "D",
    "B",
};

static size_t convsize[] = {
    sizeof(int16_t),
    sizeof(uint16_t),
    sizeof(int32_t),
    sizeof(uint32_t),
    sizeof(float),
    sizeof(double),
    sizeof(int16_t),
};

//
typedef struct {
    int      cpu;
    uint8_t  type;
    uint32_t addr;
    width_t  width;
    int      nleft;
    int      noff;
    int      spmod;
    int      m_unit;
    int      m_slot;
    conv_t   conv;
    void    *val;    //
} YEW_PLC;

//
static void *yew_calloc(int, uint8_t, uint32_t, width_t);
static long yew_parse_link(DBLINK *, struct sockaddr_in *, uint32_t *, void *);
static long yew_config_command(uint8_t *, int *, void *, int, int, uint32_t *, dbCommon *);
static long yew_parse_response(uint8_t *, int *, void *, int, int, uint32_t *, dbCommon *);

//
//
//
static void *yew_calloc(int cpu, uint8_t type, uint32_t addr, width_t width)
{
    YEW_PLC *d = calloc(1, sizeof(YEW_PLC));
    if (!d) {
        errlogPrintf("devYewPlc: calloc failed\n");
        return NULL;
    }

    d->cpu   = cpu;
    d->type  = type;
    d->addr  = addr;
    d->width = width;  // d->width might be modified in yew_parse_link()
    d->conv  = kSHORT; // d->conv migh be modified in yew_parse_link()
    d->val   = 0;      // d->val will be set in devWaveformYewPlc/devArrayoutYewPlc
    return d;
}

#include "devAiYewPlc.c"
#include "devAoYewPlc.c"
#include "devBiYewPlc.c"
#include "devBoYewPlc.c"
#include "devLonginYewPlc.c"
#include "devLongoutYewPlc.c"
#include "devMbbiYewPlc.c"
#include "devMbboYewPlc.c"
#include "devMbbiDirectYewPlc.c"
#include "devMbboDirectYewPlc.c"
#include "devWaveformYewPlc.c"
#include "devArrayoutYewPlc.c"
#include "devPatternYewPlc.c"
#include "devStatusYewPlc.c"

//
// Link field parser for both command/response I/O and event driven I/O
//
static long yew_parse_link(DBLINK             *plink,
                           struct sockaddr_in *peer_addr,
                           uint32_t           *option,
                           void               *device
                           )
{
    char *protocol = NULL;
    char *route = NULL;
    char *cpu   = NULL;
    char *type  = NULL;
    char *addr  = NULL;
    char *lopt  = NULL;
    YEW_PLC *d  = device;

    if (parseLinkPlcCommon(plink,
                           peer_addr,
                           &protocol,
                           &route, // dummy
                           &cpu,   // passed as 'unit' in parseLinkPlcCommon()
                           &type,
                           &addr,
                           &lopt
                           )) {
        errlogPrintf("devYewPlc: illegal input specification\n");
        return ERROR;
    }

    if (!peer_addr->sin_port) {
        peer_addr->sin_port = htons(yewPort);
        LOGMSG("devYewPlc: port: 0x%04x\n", ntohs(peer_addr->sin_port));
    }

    if (protocol) {
        if (strncasecmp(protocol, "TCP", 3) == 0) {
            setTcp(*option);
        } else if (strncasecmp(protocol, "UDP", 3) == 0) {
            setUdp(*option);
        } else {
            errlogPrintf("devYewPlc: unrecognized protocol\n");
            return ERROR;
        }
    }

    if (cpu) {
        if (strncmp(cpu, "0x", 2) == 0) {
            if (sscanf(cpu, "%x", &d->cpu) != 1) {
                errlogPrintf("devYewPlc: can't get CPU number\n");
                return ERROR;
            }
        } else {
            if (sscanf(cpu, "%d", &d->cpu) != 1) {
                errlogPrintf("devYewPlc: can't get CPU number\n");
                return ERROR;
            }
        }
    } else {
        d->cpu = YEW_DEFAULT_CPU;
    }
    LOGMSG("devYewPlc: cpu: %d\n", d->cpu);

    if (!addr) {
        errlogPrintf("devYewPlc: no address specified\n");
        return ERROR;
    }

    if (type) {
        if (isalpha(type[0])) {
            // CPU Module or Digital I/O Module
            if (sscanf(type, "%c", &d->type) != 1) {
                errlogPrintf("devYewPlc: can't get device type\n");
                return ERROR;
            }
        }
#ifdef YEW_SPECIAL_MODULE
        else {
            // Special Module
            if (sscanf(type, "%d,%d", &d->m_unit, &d->m_slot) == 2) {
                // module-unit number and slot number
            } else if (sscanf(type, "%d", &d->m_slot) == 1) {
                // slot number only
                d->m_unit = YEW_DEFAULT_MODULE_UNIT;
            } else {
                errlogPrintf("devYewPlc: can't get module unit and/or slot number\n");
                return ERROR;
            }

            d->spmod = 1;
        }
#endif
        if (strncmp(addr, "0x", 2) == 0) {
            if (sscanf(addr, "%x", &d->addr) != 1) {
                errlogPrintf("devYewPlc: can't get device addr\n");
                return ERROR;
            }
        } else {
            if (sscanf(addr, "%d", &d->addr) != 1) {
                errlogPrintf("devYewPlc: can't get device addr\n");
                return ERROR;
            }
        }
    } else {
        // for backward compatibility
        // no delimiter (:) between "type" and "addr"
        if (strncmp(addr + 1, "0x", 2) == 0) {
            if (sscanf(addr, "%c%x", &d->type, &d->addr) != 2) {
                errlogPrintf("devYewPlc: can't get device type or addr\n");
                return ERROR;
            }
        } else {
            if (sscanf(addr, "%c%d", &d->type, &d->addr) != 2) {
                errlogPrintf("devYewPlc: can't get device type or addr\n");
                return ERROR;
            }
        }
    }

    if (lopt) {
        //printf("devYewPlc: lopt: \"%s\"\n", lopt);

        if (0) {
            //
        } else if (lopt[0] == 'D') {
            d->conv =  kDOUBLE;
            d->width = kQWord;
            LOGMSG("devYewPlc: found option to handle the data as double precision floating point data\n");
        } else if (lopt[0] == 'F') {
            d->conv  = kFLOAT;
            d->width = kDWord;
            LOGMSG("devYewPlc: found option to handle the data as single precision floating point data\n");
        } else if (lopt[0] == 'L') {
            d->conv  = kLONG;
            d->width = kDWord;
            LOGMSG("devYewPlc: found option to handle the data as long word data\n");
        } else if (lopt[0] == 'U') {
            d->conv  = kUSHORT;
            d->width = kWord;
            LOGMSG("devYewPlc: found option to handle the data as unsigned data\n");
        } else if (lopt[0] == 'B') {
            d->conv  = kBCD;
            d->width = kWord;
            LOGMSG("devYewPlc: found option to handle the data as BCD data\n");
        } else {
            errlogPrintf("devYewPlc: unsupported conversion: %s\n", lopt);
            return ERROR;
        }
    }

    if (!d->spmod) {
        // CPU Module or Digital I/O Module
        switch (d->width) {
        case kBit:
            switch (d->type) {
            case  'X':    // input relay
                if (isWrite(*option)) {
                    errlogPrintf("devYewPlc: write access to read-only device\n");
                    return ERROR;
                }
            case  'Y':    // output relay
            case  'I':    // internal relay
            case  'E':    // shared relay
            case  'M':    // special relay
            case  'T':    // timer relay
            case  'C':    // counter relay
            case  'L':    // link relay
                break;
            default:
                errlogPrintf("devYewPlc: illegal device specification \'%c\' (width %d)\n", d->type, d->width);
                return ERROR;
            }
            break;
        case kWord:
        case kDWord:
        case kQWord:
            switch (d->type) {
            case  'X':    // input relay
            case 0x20:    // timer set
            case 0x30:    // counter set
                if (isWrite(*option)) {
                    errlogPrintf("devYewPlc: write access to read-only device\n");
                    return ERROR;
                }
            case  'Y':    // output relay
            case  'I':    // internal relay
            case  'E':    // shared relay
            case  'M':    // special relay - accessing special relays with 16-bit width data type may cause error within CPU.
            case  'T':    // timer relay
            case  'C':    // counter relay
            case  'L':    // link relay
            case  'D':    // data register
            case  'B':    // file register
            case  'F':    // cache register
            case  'R':    // shared register
            case  'V':    // index register
            case  'Z':    // special register
            case  'W':    // link register
            case 0x61:    // timer current
            case 0x65:    // timer current (count up)
            case 0x70:    // counter set
            case 0x71:    // counter current
            case 0x75:    // counter current (count up)
                break;
            default:
                errlogPrintf("devYewPlc: unknown device type \'%c\'\n", d->type);
                return ERROR;
            }
        break;
        default:
            errlogPrintf("devYewPlc: illegal data width %d\n", d->width);
            return ERROR;
        }

        d->type = d->type - '@'; // subtract 0x40 to get data class

    } else {
        // Special Module
        if (d->m_slot < 1 || d->m_slot > 16) {
            errlogPrintf("devYewPlc: slot number (%d) out of range\n", d->m_slot);
            return ERROR;
        }
    }

    return OK;
}

//
// Command constructor for command/response I/O
//
static long yew_config_command(uint8_t  *buf,    // driver buf addr
                               int      *len,    // driver buf size
                               void     *bptr,   // record buf addr
                               int       ftvl,   // record field type
                               int       ndata,  // number of elements to be transferred
                               uint32_t *option, // direction etc.
                               dbCommon *prec
                               )
{
    TRANSACTION *t = prec->dpvt;
    PEER        *p = t->facility;
    YEW_PLC     *d = t->device;

    //errlogPrintf("%s: %s PEER=%p %d\n", __func__, prec->name, p, p->timeout_state);

    // Send a test command to ensure proper recovery from timeout
    if (p && p->timeout_state) {
        *((uint8_t  *)&buf[ 0]) = 0x51;                     // Test Command
        *((uint8_t  *)&buf[ 1]) = d->cpu;                   // CPU No.
        *((uint16_t *)&buf[ 2]) = htons(sizeof(uint32_t));  // number of data below
        *((uint32_t *)&buf[ 4]) = htonl(p->timeout_count);  // sequence numnber
        *len = 4 + sizeof(uint32_t);

        //DEBUG
        //errlogPrintf("devYewPlc: %s sending test command to recover from timeout: id=0x%04x cmd=0x%02x\n", prec->name, p->timeout_count, buf[0]);
        return 0;
    }

    //
    const int nbytes = (d->width)*ndata;

    //DEBUG
    if (netDevDebug>0) {
    //if (bptr) { // don't show debug message for read request
        printf("%s: %s (buf=%8p len=%4d bptr=%8p ftvl=%d ndata=%4d width=%d nbytes=%d nleft=%d noff=%d isWrite=%d)\n", __FILE__, __func__, buf, *len, bptr, ftvl, ndata, d->width, nbytes, d->nleft, d->noff, isWrite(*option));
    //}
    }

    int nread = nbytes;

    // Read/Write request exceeding the maximum will be read/written for the next time
    if (nbytes > YEW_MAX_BYTES) {
        if (!d->noff) {
            // for the first time
            d->nleft = nbytes;
        }

        nread = (d->nleft > YEW_MAX_BYTES) ? YEW_MAX_BYTES : d->nleft;
    }

    int nwrite = isWrite(*option) ? nread : 0;

    if (*len < YEW_CMND_LENGTH(d->spmod) + nwrite) {
        errlogPrintf("devYewPlc: buffer is running short\n");
        return ERROR;
    }

    uint8_t  command_type;
    uint16_t bytes_follow;
    int      offset;
    int      npoint;

    if (!d->spmod) {
        // CPU Module or Digital I/O Module
        switch (d->width) {
        case kBit:  // bit device
            command_type = isRead(*option)? 0x01:0x02;
            bytes_follow = isRead(*option)? htons(0x0008):htons(0x0008 + nwrite);
            offset       = d->noff;
            npoint       = nread;
            break;
        case kWord:  // word device
        case kDWord: // word device x2
        case kQWord: // word device x4
            command_type = isRead(*option)? 0x11:0x12;
            bytes_follow = isRead(*option)? htons(0x0008):htons(0x0008 + nwrite);
            offset       = d->noff / 2;
            npoint       = nread / 2;
            break;
        default:
            errlogPrintf("devYewPlc: unsupported data width: addr:%d width:%d\n", d->addr, d->width);
            return ERROR;
        }

        *((uint8_t  *)&buf[ 0]) = command_type;             // R/W by bit/word
        *((uint8_t  *)&buf[ 1]) = d->cpu;                   // CPU No.
        *((uint16_t *)&buf[ 2]) = bytes_follow;             // number of data below
        *((uint16_t *)&buf[ 4]) = htons(d->type);           // device type
        *((uint32_t *)&buf[ 6]) = htonl(d->addr + offset);  // device addr
        *((uint16_t *)&buf[10]) = htons(npoint);            // number of data

        //Debug
        //errlogPrintf("devYewPlc: %s: %s [%c] %c%04d\n", __func__, prec->name, isWrite(*option)?'W':'R', d->type+'@', d->addr+offset);
    } else {
        // Special Module, "type" holds module unit/slot number
        command_type = isRead(*option)? 0x31:0x32;
        bytes_follow = isRead(*option)? htons(0x0006):htons(0x0006 + nwrite);
        offset       = d->noff / 2; // need check - do we have other than word device?
        npoint       = nread / 2;   // need check - do we have other than word device?

        *((uint8_t  *)&buf[ 0]) = command_type;             // R/W by bit/word
        *((uint8_t  *)&buf[ 1]) = d->cpu;                   // CPU No.
        *((uint16_t *)&buf[ 2]) = bytes_follow;             // number of data below
        *((uint8_t  *)&buf[ 4]) = d->m_unit;                // module unit
        *((uint8_t  *)&buf[ 5]) = d->m_slot;                // module slot
        *((uint16_t *)&buf[ 6]) = htons(d->addr + offset);  // data position
        *((uint16_t *)&buf[ 8]) = htons(npoint);            // number of data
    }

    //DEBUG
    if (netDevDebug>0) {
        printf("                     [req] => nbytes=%4d (npoint=%4d) @ addr=%4d [nleft=%4d noff=%4d]\n", nread, npoint, d->addr+offset, d->nleft, d->noff);
    }

    if (isWrite(*option)) {
        const int offset = d->noff / d->width;
        const int npoint = nread / d->width;

        if (fromRecordVal(&buf[YEW_CMND_LENGTH(d->spmod)],
                          d->width,
                          bptr,
                          offset,
                          ftvl,
                          npoint,
                          YEW_NEEDS_SWAP
                          )) {
            errlogPrintf("devYewPlc: illegal command\n");
            return ERROR;
        }
    }

    *len = YEW_CMND_LENGTH(d->spmod) + nwrite;

    //nread  = isRead(*option)? (d->width)*n:0;
    //return (YEW_DATA_OFFSET + nread);

    return 0;
}

//
// Response parser for command/response I/O
//
static long yew_parse_response(uint8_t  *buf,    // driver buf addr
                               int      *len,    // driver buf size
                               void     *bptr,   // record buf addr
                               int       ftvl,   // record field type
                               int       ndata,  // number of elements to be transferred
                               uint32_t *option, // direction etc.
                               dbCommon *prec
                               )
{
    TRANSACTION *t = prec->dpvt;
    PEER        *p = t->facility;
    YEW_PLC     *d = t->device;

    // Special treatment for timeout
    if (p && p->timeout_state) {
        if (buf[0] != 0xd1) {
            // A normal response was received during recovery from a timeout.
            // It seems that the response was simply delayed.
            // We return immediately without interpreting the received data, otherwise d->noff may increase, causing a displacement in the waveform data.

            //DEBUG
            //errlogPrintf("devYewPlc: Received a response that was NOT from test command: Expected=0x%02x Received=0x%02x len=%d)\n", 0xd1, buf[0], *len);
            return NOT_MINE;
        }

        uint16_t num = ntohs(*((uint16_t *)&buf[2]));
        if (num != sizeof(uint32_t)) {
            // A response from test command was received, but number of data does not match.
            // This may not happen.
            errlogPrintf("devYewPlc: Number of data does not match: Expected=%Zu Received=%u\n",
                         sizeof(uint32_t), num);
            return NOT_MINE;
        }

        uint32_t received = ntohl(*((uint32_t *)&buf[4]));
        if (p->timeout_count != received) {
            // A response from the test command was recived, but the sequence number does not match.
            // This may not happen.
            errlogPrintf("devYewPlc: Sequence number mismatch: Expected=0x%04x Received=0x%04x\n", p->timeout_count, received);
            return NOT_MINE; // ERROR?
        } else {
            // Received a response from the test command, i.e. successfully recovered from the timeout

            //DEBUG
            //errlogPrintf("devYewPlc: Received a response to test command; Successfully recovered from timeout: Expected=0x%04x Received=0x%04x\n", p->timeout_count, received);
            return RECOVERED;
        }

        //DEBUG
        //errlogPrintf("devYewPlc: %s: %s [0x%02x] len=%d\n", __func__, prec->name, buf[0], *len);
    }

//    LOGMSG("devYewPlc: yew_parse_response(%8p,%d,%8p,%d,%d,%d,%8p)\n", buf, *len, bptr, ftvl, ndata, *option, d);
    const int nbytes = (d->width)*ndata;

    //DEBUG
    if (netDevDebug>0) {
        //if (bptr) { // don't show debug message for write request
        printf("%s: %s (buf=%8p len=%4d bptr=%8p ftvl=%d ndata=%4d width=%d nbytes=%d nleft=%d noff=%d isWrite=%d)\n", __FILE__, __func__, buf, *len, bptr, ftvl, ndata, d->width, nbytes, d->nleft, d->noff, isWrite(*option));
        //}
    }

    int nread = nbytes;
    int ret = 0;

    // Read request exceeding the maximum will be read for the next time
    if (nread > YEW_MAX_BYTES) {
        nread = (d->nleft > YEW_MAX_BYTES) ? YEW_MAX_BYTES : d->nleft;
        ret   = (d->nleft > YEW_MAX_BYTES) ? NOT_DONE : 0;
    }

    uint8_t  response_type;
    uint16_t number_of_data;

    if (!d->spmod) {
        // CPU Module or Digital I/O Module
        switch (d->width) {
        case kBit:  // bit device
            response_type  = isRead(*option)? 0x81:0x82;
            number_of_data = isRead(*option)? htons(nread):htons(0x0000);
            break;
        case kWord:  // word device
        case kDWord: // word device x2
        case kQWord: // word device x4
            response_type  = isRead(*option)? 0x91:0x92;
            number_of_data = isRead(*option)? htons(nread):htons(0x0000);
            break;
        default:
            errlogPrintf("devYewPlc: unsupported data width\n");
            return ERROR;
        }
    } else {
        // Special Module
        response_type  = isRead(*option)? 0xB1:0xB2;
        number_of_data = isRead(*option)? htons(nread):htons(0x0000);
    }

    if (buf[0] != response_type) {
        errlogPrintf("devYewPlc: Unexpected response: Expected=0x%02x Received=0x%02x len=%d)\n", response_type, buf[0], *len);
        return NOT_MINE;
    }

    if (buf[1] != 0x00) {
        errlogPrintf("devYewPlc: error termination code returned (0x%02x)\n", buf[1]);
        return ERROR;
    }

    if (*((uint16_t *)&buf[2]) != number_of_data) {
        errlogPrintf("devYewPlc: number of data does not match: Expected=%d Received=%d\n",
                     number_of_data, *((uint16_t *)&buf[2]));
        return NOT_MINE;
    }

    if (isRead(*option)) {
        const int npoint = nread / d->width;
        const int offset = d->noff / d->width;

        //DEBUG
        if (netDevDebug>0) {
            printf("                     [fil] => nbytes=%4d (npoint=%4d) @ addr=%4d [nleft=%4d noff=%4d]\n", nread, npoint, d->addr+offset, d->nleft, d->noff);
        }

        if (toRecordVal(bptr,
                        offset,
                        ftvl,
                        &buf[YEW_DATA_OFFSET],
                        d->width,
                        npoint,
                        YEW_NEEDS_SWAP
                        )) {
            errlogPrintf("devYewPlc: illegal or error response\n");
            ret = ERROR;
        }
    }

    // Number of remaining data and its staring position for the next time
    d->nleft -= nread;
    d->noff  += nread;

    if (d->nleft <= 0) {
        // No remaining data
        d->nleft = 0; // do we need this?
        //d->noff = 0; // noff will be cleared in read/write record functions
    }

    //DEBUG
    if (netDevDebug>0) {
        printf("                     [ret] => ret=%4d                              [nleft=%4d noff=%4d]\n", ret, d->nleft, d->noff);
    }

    return ret;
}

//
// IOC shell command registration
//
#include <iocsh.h>

// Switch the Yew Plc default protocol (UDP or TCP)
int yewPlcProtocol(char *str);
static const iocshArg yewPlcProtocolArg0 = { "protocol", iocshArgString };
static const iocshArg *yewPlcProtocolArgs[] = { &yewPlcProtocolArg0 };
static const iocshFuncDef yewPlcProtocolFuncDef = {"yewPlcProtocol", 1, yewPlcProtocolArgs,
    "Switches the Yew Plc protocol default to UDP or TCP (case insensitive).\n"
    "The protocol specified in INP/OUT field takes precedece.\n"
    "Calling yewPlcProtocol after iocInit has no effect.\n"
    "Default protocol: UDP\n"
};

static void yewPlcProtocolCallFunc(const iocshArgBuf *args) {
    if (! yewPlcProtocol(args[0].sval)) {
        iocshSetError(-1);
    }
}

int yewPlcProtocol(char *protocol)
{
    if (!protocol) {
        printf("Yew Plc Protocol : %s\n", (yewProtocol==MPF_TCP) ? "TCP" : "UDP" );
        return 0;
    } else if (strncasecmp(protocol, "TCP", 3) == 0) {
        yewProtocol = MPF_TCP;
        return 1;
    } else if (strncasecmp(protocol, "UDP", 3) == 0) {
        yewProtocol = MPF_UDP;
        return 1;
    } else {
        printf("Error : Unrecognized protocol: %s\n", protocol);
        return 0;
    }
}

// Swhtch the Yew Plc default port number (0x3001 or 0x3003)
int yewPlcPort(unsigned port);
static const iocshArg yewPlcPortArg0 = { "port#", iocshArgInt };
static const iocshArg *yewPlcPortArgs[] = { &yewPlcPortArg0 };
static const iocshFuncDef yewPlcPortFuncDef = {"yewPlcPort", 1, yewPlcPortArgs,
    "Switches the Yew Plc port number default to 0x3001(12889; for Port-A) or 0x3003(12291; for Port-B).\n"
    "The port number specified in INP/OUT field takes precedece.\n"
    "Calling yewPlcPort after iocInit has no effect.\n"
    "Default port number: 0x3001\n"
};

static void yewPlcPortCallFunc(const iocshArgBuf *args) {
    if (! yewPlcPort(args[0].ival)) {
        iocshSetError(-1);
    }
}

int yewPlcPort(unsigned port)
{
    if (port == 0) {
        printf("Yew Plc Port : %u(0x%04x)\n", yewPort, yewPort);
        return 0;
    } else if (port == 0x3001) {
        yewPort = port;
        return 1;
    } else if (port == 0x3003) {
        yewPort = port;
        return 1;
    } else {
        printf("Error : Unrecognized port#: %u(%04x)\n\n", port, port);
        return 0;
    }
}

// Change Yew Plc timeout for send()/sendto()
int yewPlcSendTimeout(char *str);
static const iocshArg yewPlcSendTimeoutArg0 = { "sendTimeout", iocshArgString };
static const iocshArg *yewPlcSendTimeoutArgs[] = { &yewPlcSendTimeoutArg0 };
static const iocshFuncDef yewPlcSendTimeoutFuncDef = {"yewPlcSendTimeout", 1, yewPlcSendTimeoutArgs,
    "Changes send timeout in seconds for the Yew Plc.\n"
    "Calling yewPlcSendTimeout after iocInit has no effect.\n"
    "Default send timeout: 0.0\n"
};

static void yewPlcSendTimeoutCallFunc(const iocshArgBuf *args) {
    if (! yewPlcSendTimeout(args[0].sval)) {
        iocshSetError(-1);
    }
}

int yewPlcSendTimeout(char *str)
{
    if (!str) {
        printf("Yew Plc SendTimeout : %.6f\n", yewSendTimeout );
        return 0;
    } else if (sscanf(str, " %lf ", &yewSendTimeout) == 1) {
        // OK
        return 1;
    } else {
        printf("Error : %s is not valid value\n", str);
        return 0;
    }
}

// Change Yew Plc timeout for recv()/recvfrom()
int yewPlcRecvTimeout(char *str);
static const iocshArg yewPlcRecvTimeoutArg0 = { "recvTimeout", iocshArgString };
static const iocshArg *yewPlcRecvTimeoutArgs[] = { &yewPlcRecvTimeoutArg0 };
static const iocshFuncDef yewPlcRecvTimeoutFuncDef = {"yewPlcRecvTimeout", 1, yewPlcRecvTimeoutArgs,
    "Changes recv timeout in seconds for the Yew Plc.\n"
    "Calling yewPlcRecvTimeout after iocInit has no effect.\n"
    "Default recv timeout: 1.0\n"
};

static void yewPlcRecvTimeoutCallFunc(const iocshArgBuf *args) {
    if (! yewPlcRecvTimeout(args[0].sval)) {
        iocshSetError(-1);
    }
}

int yewPlcRecvTimeout(char *str)
{
    if (!str) {
        printf("Yew Plc RecvTimeout : %.6f\n", yewRecvTimeout );
        return 0;
    } else if (sscanf(str, " %lf ", &yewRecvTimeout) == 1) {
        // OK
        return 1;
    } else {
        printf("Error : %s is not valid value\n", str);
        return 0;
    }
}

// Change Yew Plc timeout using EpicsTimer (deprecated)
int yewPlcEpicsTimerTimeout(char *str);
static const iocshArg yewPlcEpicsTimerTimeoutArg0 = { "epicsTimerTimeout", iocshArgString };
static const iocshArg *yewPlcEpicsTimerTimeoutArgs[] = { &yewPlcEpicsTimerTimeoutArg0 };
static const iocshFuncDef yewPlcEpicsTimerTimeoutFuncDef = {"yewPlcEpicsTimerTimeout", 1, yewPlcEpicsTimerTimeoutArgs,
    "Changes epicsTimer timeout in seconds for the Yew Plc..\n"
    "EpicsTimer timeout is kept for backward compatibility and timeouts specified by yewPlcSendTimeout/yewPlcRecvTimeout take precedece.\n"
    "Calling yewPlcEpicsTimerTimeout after iocInit has no effect.\n"
    "Default EpicsTimer timeout: 0.0\n"
};

static void yewPlcEpicsTimerTimeoutCallFunc(const iocshArgBuf *args) {
    if (! yewPlcEpicsTimerTimeout(args[0].sval)) {
        iocshSetError(-1);
    }
}

int yewPlcEpicsTimerTimeout(char *str)
{
    if (!str) {
        printf("Yew Plc EpicsTimeout : %.6f\n", yewEpicsTimerTimeout );
        return 0;
    } else if (sscanf(str, " %lf ", &yewEpicsTimerTimeout) == 1) {
        // OK
        return 1;
    } else {
        printf("Error : %s is not valid value\n", str);
        return 0;
    }
}

//
static void devYewPlcRegisterCommands(void);
static void devYewPlcRegisterCommands(void)
{
    //DEBUG
    //errlogPrintf("devYewPlc: %s\n", __func__);

    static int init_flag = 0;
    if (!init_flag) {
        init_flag = 1;
        iocshRegister(&yewPlcProtocolFuncDef, yewPlcProtocolCallFunc);
        iocshRegister(&yewPlcPortFuncDef, yewPlcPortCallFunc);
        iocshRegister(&yewPlcRecvTimeoutFuncDef, yewPlcRecvTimeoutCallFunc);
        iocshRegister(&yewPlcSendTimeoutFuncDef, yewPlcSendTimeoutCallFunc);
        iocshRegister(&yewPlcEpicsTimerTimeoutFuncDef, yewPlcEpicsTimerTimeoutCallFunc);
    }
}

//
epicsExportRegistrar(devYewPlcRegisterCommands);

// end
