/* devChanDarwin.c */
/****************************************************************************
 *                         COPYRIGHT NOTIFICATION
 *
 * Copyright (c) All rights reserved
 *
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 ****************************************************************************/
/* Author: Jun-ichi Odagiri*/
/* Modification Log:
 * -----------------
 */

#include <epicsExport.h>
#include <channelsRecord.h>

/* cmtout = not debub
#define DEBUG
 */
#define MAX_NUM_CHANNELS  90

/***************************************************************
 * Response parser
 ***************************************************************/
LOCAL long init_chan_record(struct channelsRecord *);
LOCAL long read_chan(struct channelsRecord *);
LOCAL long config_chan_command(struct dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_chan_response(struct dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devChannelsDarwin = {
    5,
    NULL,
    NULL,
    init_chan_record,
    netDevGetIoIntInfo,
    read_chan
};

epicsExportAddress(dset, devChannelsDarwin);
extern void swap_bytes(void *ptr, int num);

LOCAL long init_chan_record(struct channelsRecord *pchan)
{
    DARWIN *d = darwin_calloc();

    LOGMSG("devChanDarwin: init_chan_record(\"%s\")\n", pchan->name);

    if (netDevInitXxRecord((struct dbCommon *) pchan,
                           &pchan->inp,
                           MPF_READ | MPF_TCP | DARWIN_TIMEOUT,
                           d,
                           darwin_parse_link,
                           config_chan_command,
                           parse_chan_response
                           )) {
        return ERROR;
    }

    if (pchan->nelm > MAX_NUM_CHANNELS) {
        errlogPrintf("devDarwin: too many number of elements\n");
        return ERROR;
    } else if (!pchan->nelm) {
        pchan->nelm = 1;
    }

    strcpy(pchan->mode, "EF");

    return OK;
}

LOCAL long read_chan(struct channelsRecord *pchan)
{
    LOGMSG("devChanDarwin: read_chan_record(\"%s\")\n", pchan->name);

    return netDevReadWriteXx((struct dbCommon *) pchan);
}

#define DATA_LENGTH_OFFSET     0
#define TIME_STAMP_OFFSET      2
#define CAHNNEL_OFFSET(x,y)  ((DATA_LENGTH_OFFSET + TIME_STAMP_OFFSET) + 8 + ((y)? 6:4)*(x))
#define DATA_OFFSET(x,y)      (CAHNNEL_OFFSET(x,y) + ((y)? 4:2))
#define ALARM_MSG_SIZE         MAX_STRING_SIZE
#define EL_RESP_LENGTH         15

LOCAL long config_chan_command(struct dbCommon *pxx,
                               int *option,
                               uint8_t *buf,
                               int *len,
                               void *device,
                               int transaction_id
                               )
{
    struct channelsRecord *pchan = (struct channelsRecord *)pxx;
    DARWIN *d = (DARWIN *) device;
    int nelm = pchan->nelm;
    int alst = pchan->alst;
    int resp_length = 0;
    int nbytes = 0;
    uint8_t *command;

    LOGMSG("devChanDarwin: config_chan_command(\"%s\")\n", pxx->name);

    if (strncmp(pchan->mode, "EF", 2) == 0) {
        command = d->comm_EF;
        if (pchan->alst) {
            command[2] = '1';
        } else {
            command[2] = '0';
        }
        resp_length = CAHNNEL_OFFSET(nelm, alst);
    } else if (strncmp(pchan->mode, "EL", 2) == 0) {
        command = d->comm_EL;
        resp_length = 15 * pchan->nelm;
    } else if (strncmp(pchan->mode, "EB", 2) == 0) {
        command = d->comm_EB;
        if (pchan->bodr) {
            command[2] = '1';
        } else {
            command[2] = '0';
        }
        resp_length = 4;
    } else {
        errlogPrintf("devDarwin: unsupported channel data command in MODE\n");
        return ERROR;
    }

    if (*len < (nbytes = strlen((char *)command))) {
        errlogPrintf("devDarwin: buffer is running short\n");
        return ERROR;
    }

    memcpy(buf, command, strlen((char *)command));
    *len = nbytes;

    return resp_length;
}

LOCAL long parse_chan_response(struct dbCommon *pxx,
                               int *option,
                               uint8_t *buf,
                               int *len,
                               void *device,
                               int transaction_id
                               )
{
    struct channelsRecord *pchan = (struct channelsRecord *)pxx;
    DARWIN *d = (DARWIN *)device;
    double *bptr = (double *) &pchan->ch01;
    char   *albp = (char   *) &pchan->al01;
    char   *eubp = (char   *) &pchan->eu01;
    short  *ppbp = (short  *) &pchan->pp01;
    uint8_t *p;
    int nelm = pchan->nelm;
    int alst = pchan->alst;
    int data_length = CAHNNEL_OFFSET(nelm, alst) - TIME_STAMP_OFFSET;
    int n, m;

    LOGMSG("devChanDarwin: parse_chan_response(%8p,0x%08x,%8p,%d,%8p,%d)\n", pxx, *option, buf, *len, device, transaction_id);

    if (strncmp(pchan->mode, "EF", 2) == 0) {
        p = buf + DATA_LENGTH_OFFSET;
        if (DARWIN_NEEDS_SWAP) {
            /* printf("swapping Data Length\n"); */
            swap_bytes(p, 1);
        }

        if (p == 0) {
            errlogPrintf("parse_chan_response: found zero Data Length (\"%s\")\n", pchan->name);
            return ERROR;
        }

        if ( *((uint16_t *) p) != data_length ) {
            errlogPrintf("parse_chan_response: Data Length dose not match: %d, %d, %d, %d(\"%s\")\n",
                         *((uint16_t *) p), data_length, nelm, alst, pchan->name);
            return ERROR;
        }

        p = buf + TIME_STAMP_OFFSET;
#ifdef vxWorks
        sprintf(pchan->tsmp, "%02d/%02d/%02d/%02d/%02d/%02d.%d",
                p[0], p[1], p[2], p[3], p[4], p[5], p[6]);
#else
        snprintf(pchan->tsmp, MAX_STRING_SIZE, "%02d/%02d/%02d/%02d/%02d/%02d.%d",
                 p[0], p[1], p[2], p[3], p[4], p[5], p[6]);
#endif
        for (n = 0; n < nelm; n++) {
            p = buf + CAHNNEL_OFFSET(n,alst); /* p points to Unit No. */

            if ( d->unit && (*p != d->unit) ) {
                errlogPrintf("parse_chan_response: read Unit No.(%d) dose not match with specified(%d) (\"%s\")\n",
                             *p, d->unit, pchan->name);
                return ERROR;
            }

            p++; /* p points to Chan No. */

            /* can not do consistency check without knowing the configuration of channels on the device */
            /*
            if ( *p != (uint8_t) (d->p1 + n) ) {
                errlogPrintf("parse_chan_response: Chan No.dose not match, %d, %d, %d(\"%s\")\n",
                             *p, d->p1 + n, d->p1, pchan->name);
                return ERROR;
            }
            */

            if (pchan->alst) {
                p++; /* p points to Alarm Sts */
#ifdef vxWorks
                sprintf(((uint8_t *) albp) + ALARM_MSG_SIZE*( n ),
                        "L1: %d, L2: %d, L3: %d, L4: %d",
                        (p[0]) & 0x0f, (p[0] >> 4) & 0x0f,
                        (p[1]) & 0x0f, (p[1] >> 4) & 0x0f);
#else
                snprintf(albp + ALARM_MSG_SIZE*( n ),
                         MAX_STRING_SIZE,
                         "L1: %d, L2: %d, L3: %d, L4: %d",
                         (p[0]) & 0x0f, (p[0] >> 4) & 0x0f,
                         (p[1]) & 0x0f, (p[1] >> 4) & 0x0f);
            }
#endif
            p = buf + DATA_OFFSET(n,alst); /* p points to Data on CH */
            if (DARWIN_NEEDS_SWAP) {
                swap_bytes(p, 1);
            }
            bptr[n] = (double) *((short *) p);

            for (m = ((unsigned short *) ppbp)[n]; m > 0; m--) {
                bptr[n] /= 10.0;
            }
        }
        pchan->nord = n;
    } else if (strncmp(pchan->mode, "EL", 2) == 0) {
        int remain = pchan->nelm;
        char sps, sts;
        int chan;
        char unit[6 + 1];
        int pos;
        int n = 0;

        if (strncmp((char *)buf, "E1", 2) == 0) {
            errlogPrintf("parse_chan_response: got error response (E1)\n");
            strcpy(pchan->mode, "EF");
            return ERROR;
        }

        do {
            if (sscanf((char *)buf, "%c%c%3d%6c,%1d\r\n", &sps, &sts, &chan, unit, &pos) != 5) {
                errlogPrintf("parse_chan_response:EL failed to read Unit/Position\n");
                strcpy(pchan->mode, "EF");
                return ERROR;
            }

            unit[6] = '\0';

            /* can not do consistency check without knowing the configuration of channels on the device */
            /*
            if (chan != d->p1 + n) {
                errlogPrintf("parse_chan_response:EL channel dose not match: %d, %d\n", chan, d->p1 + n);
                strcpy(pchan->mode, "EF");
                return ERROR;
            }
            */

            if (sps != ' ') {
                LOGMSG("parse_chan_response:EL leading char is not SPACE(\"%s\", 0x%02x, %d)\n", pchan->name, sts,chan);
                strcpy(pchan->mode, "EF");
                return ERROR;
            }

            if (remain == 1) {
                if (sts != 'E') {
                    LOGMSG("parse_chan_response:EL not found END marker(\"%s\", 0x%02x, %d)\n", pchan->name, sts, chan);
                    strcpy(pchan->mode, "EF");
                    return ERROR;
                }
            } else {
                if (sts != ' ') {
                    LOGMSG("parse_chan_response:EL found unexpected marker(\"%s\", 0x%02x, %d)\n", pchan->name, sts,chan);
                    strcpy(pchan->mode, "EF");
                    return ERROR;
                }
            }

            strcpy(eubp + 8 * ( n ), unit);
            ppbp[n] = (short) pos;

            n++;
            buf += EL_RESP_LENGTH;

        } while (--remain);

        strcpy(pchan->mode, "EF");
    } else {
        int code;

        if (sscanf((char *)buf, "E%d\r\n", &code) != 1) {
            errlogPrintf("parse_chan_response: failed to read returned error code\n");
            strcpy(pchan->mode, "EF");
            return ERROR;
        }

        if (code) {
            errlogPrintf("parse_chan_response: error code %d returned\n", code);
            strcpy(pchan->mode, "EF");
            return ERROR;
        }

        strcpy(pchan->mode, "EF");
    }

    return OK;
}
