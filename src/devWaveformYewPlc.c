/* devWaveformYewPlc.c */
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
 */

#include <waveformRecord.h>

//
// Waveform (command/response IO)
//
static long init_waveform_record(waveformRecord *);
static long read_waveform(waveformRecord *);
static long config_waveform_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_waveform_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devWfYewPlc = {
    5,
    NULL,
    netDevInit,
    init_waveform_record,
    NULL,
    read_waveform
};

epicsExportAddress(dset, devWfYewPlc);

static long init_waveform_record(waveformRecord *prec)
{
    return netDevInitXxRecord((dbCommon *)prec,
                              &prec->inp,
                              MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                              yew_calloc(0, 0, 0, kWord),
                              yew_parse_link,
                              config_waveform_command,
                              parse_waveform_response
                              );
}

static long read_waveform(waveformRecord *prec)
{
    TRANSACTION *t = prec->dpvt;
    YEW_PLC *d = t->device;

    // make sure that those below are cleared in the event that
    // a multi-step transfer is terminated by an error in the
    // middle of transacton
    d->nleft = 0;
    d->noff = 0;

    return netDevReadWriteXx((dbCommon *)prec);
}

static long config_waveform_command(dbCommon *pxx,
                                    int *option,
                                    uint8_t *buf,
                                    int *len,
                                    void *device,
                                    int transaction_id
                                    )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("\n%s: %s %s\n", __FILE__, __func__, pxx->name);
    }

    waveformRecord *prec = (waveformRecord *)pxx;

    return yew_config_command(buf,
                              len,
                              0, // not used in yew_config_command
                              0, // not used in yew_config_command
                              prec->nelm,
                              option,
                              device
                              );
}

static long parse_waveform_response(dbCommon *pxx,
                                    int *option,
                                    uint8_t *buf,
                                    int *len,
                                    void *device,
                                    int transaction_id
                                    )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("\n%s: %s %s\n", __FILE__, __func__, pxx->name);
    }

    waveformRecord *prec = (waveformRecord *)pxx;
    YEW_PLC *d = device;

    long ret = yew_parse_response(buf,
                                  len,
                                  prec->bptr,
                                  prec->ftvl, // This has nothing to do with options such as \&F or \&L
                                  prec->nelm,
                                  option,
                                  d
                                  );

    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %s %s : ret=%ld ndata=%d nleft=%d noff=%d\n", __FILE__, __func__, pxx->name, ret, prec->nelm, d->nleft, d->noff);
    }

    switch (ret) {
    case NOT_DONE:
        prec->nord = d->noff;
        // why we don't have break here?
    case 0:
        prec->nord = prec->nelm;
    default:
        ;
    }

    return ret;
}
