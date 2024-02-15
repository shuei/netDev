/* devChansMW100.c */
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

#include <epicsExport.h>
#include <chansRecord.h>

#define DEBUG

//
// Chans (command/response I/O)
//
static long init_chans_record(chansRecord *);
static long read_chans(chansRecord *);
static long config_chans_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_chans_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devChansMW100 = {
    5,
    NULL,
    NULL,
    init_chans_record,
    netDevGetIoIntInfo,
    read_chans
};

epicsExportAddress(dset, devChansMW100);

static long init_chans_record(chansRecord *pchans)
{
    LOGMSG("devChansMW100: init_chans_record(\"%s\")\n", pchans->name);

    MW100 *d = MW100_calloc();
    if (netDevInitXxRecord((dbCommon *)pchans,
                           &pchans->inp,
                           MPF_READ | MPF_TCP | MW100_TIMEOUT,
                           d,
                           MW100_parse_link,
                           config_chans_command,
                           parse_chans_response
                           )) {
        return ERROR;
    }

    pchans->noch = d->noch;

    return OK;
}

static long read_chans(chansRecord *pchans)
{
    LOGMSG("devChansMW100: read_chans_record(\"%s\")\n", pchans->name);

    return netDevReadWriteXx((dbCommon *)pchans);
}

#define RESPONSE_LENGTH(x) (4 + 15 + 15 + 28*(x) + 4)

static long config_chans_command(dbCommon *pxx,
                                 int *option,
                                 uint8_t *buf,
                                 int *len,
                                 void *device,
                                 int transaction_id
                                 )
{
    LOGMSG("devChansMW100: config_chans_command(\"%s\")\n", pxx->name);

    MW100 *d = (MW100 *)device;

    if (*len < d->com_len) {
        errlogPrintf("devMW100: buffer is running short\n");
        return ERROR;
    }

    memcpy(buf, d->com_FD, d->com_len);
    *len = d->com_len;

    chansRecord *pchans = (chansRecord *)pxx;

    return RESPONSE_LENGTH(pchans->noch);
}

static long parse_chans_response(dbCommon *pxx,
                                 int *option,
                                 uint8_t *buf,
                                 int *len,
                                 void *device,
                                 int transaction_id
                                 )
{
    LOGMSG("devChansMW100: parse_chans_response(%8p,0x%08x,%8p,%d,%8p,%d)\n", pxx, *option, buf, *len, device, transaction_id);

    chansRecord *pchans = (chansRecord *)pxx;
    MW100 *d         = (MW100 *)device;
    double *data     = (double *)&pchans->ch01;
    char  (*stat)[4] = (char (*)[4])pchans->st01;
    char  (*alrm)[8] = (char (*)[8])pchans->al01;
    char  (*unit)[8] = (char (*)[8])pchans->eu01;
    char channel[8];
    int noch = pchans->noch;
    int ichan;
    int n;
    int i;

    // clear NORD
    pchans->nord = 0;

    // terminate response message with NULL
    buf[*len] = '\0';

    // check EA
    char *pC1 = (char *)buf;
    char *pC2 = strchr(pC1, '\r');
    if (!pC2) {
        errlogPrintf("devMW100: unexpected response (no CR for \"EA\")\n");
        return ERROR;
    }
    *pC2 = '\0';
    if (strncmp(pC1, "EA", 2) != 0) {
        errlogPrintf("devMW100: unexpected response (no \"EA\")\n");
        return ERROR;
    }
    if (*(++pC2) != '\n') {
        errlogPrintf("devMW100: unexpected response (no LF for \"EA\")\n");
        return ERROR;
    }

    // get DATE
    pC1 = ++pC2;
    pC2 = strchr(pC1, '\r');
    if (!pC2) {
        errlogPrintf("devMW100: unexpected response (no CR for \"DATE\")\n");
        return ERROR;
    }

    *pC2 = '\0';
    if (sscanf(pC1, "DATE %s", &pchans->date[0]) != 1) {
        errlogPrintf("devMW100: unexpected response (can't get \"DATE\")\n");
        return ERROR;
    }

    if (*(++pC2) != '\n') {
        errlogPrintf("devMW100: unexpected response (no LF for \"DATE\")\n");
        return ERROR;
    }

    // get TIME
    pC1 = ++pC2;
    pC2 = strchr(pC1, '\r');
    if (!pC2) {
        errlogPrintf("devMW100: unexpected response (no CR for \"TIME\")\n");
        return ERROR;
    }

    *pC2 = '\0';
    if (sscanf(pC1, "TIME %s", &pchans->atim[0]) != 1) {
        errlogPrintf("devMW100: unexpected response (can't get \"TIME\")\n");
        return ERROR;
    }
    if (*(++pC2) != '\n') {
        errlogPrintf("devMW100: unexpected response (no LF for \"TIME\")\n");
        return ERROR;
    }

    for (n = 0; n < noch; n++) {
        pC1 = ++pC2;
        pC2 = strchr(pC1, '\r');
        i = d->p1 + n;

        if (!pC2) {
            errlogPrintf("devMW100: unexpected response (no CR for ch%02d)\n", i);
            return ERROR;
        }

        *pC2 = '\0';
        if (sscanf(pC1, "%c %4c%4c%6c%lf",
                   stat[n],
                   channel,
                   alrm[n],
                   unit[n],
                   &data[n]) != 5) {
            errlogPrintf("devMW100: unexpected response (can't get ch%02d)\n", i);
            return ERROR;
        }

        channel[3] = '\0';
        if (sscanf(channel, "%d", &ichan) != 1) {
            errlogPrintf("devMW100: unexpected response (can't get ch no:%02d)\n", i);
            return ERROR;
        }

        if (ichan != i) {
            errlogPrintf("devMW100: unexpected response (ch no does not match (%02d,%02d))\n",
                         ichan, i);
            return ERROR;
        }

        if (*(++pC2) != '\n') {
            errlogPrintf("devMW100: unexpected response (no LF for ch%02d)\n", i);
            return ERROR;
        }

        pchans->nord++;
    }

    // check EN
    pC1 = ++pC2;
    pC2 = strchr(pC1, '\r');
    if (!pC2) {
        errlogPrintf("devMW100: unexpected response (no CR for \"EN\")\n");
        return ERROR;
    }

    *pC2 = '\0';
    if (strncmp(pC1, "EN", 2) != 0) {
        errlogPrintf("devMW100: unexpected response (no \"EN\")\n");
        return ERROR;
    }

    if (*(++pC2) != '\n') {
        errlogPrintf("devMW100: unexpected response (no LF for \"EN\")\n");
        return ERROR;
    }

    pchans->udf = FALSE;

    return OK;
}
