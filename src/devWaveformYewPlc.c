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

static long init_waveform_record(waveformRecord *pwf)
{
    return netDevInitXxRecord((dbCommon *)pwf,
                              &pwf->inp,
                              MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                              yew_calloc(0, 0, 0, kWord),
                              yew_parse_link,
                              config_waveform_command,
                              parse_waveform_response
                              );
}

static long read_waveform(waveformRecord *pwf)
{
    TRANSACTION *t = pwf->dpvt;
    YEW_PLC *d = t->device;

    // make sure that those below are cleared in the event that
    // a multi-step transfer is terminated by an error in the
    // middle of transacton
    d->nleft = 0;
    d->noff = 0;

    return netDevReadWriteXx((dbCommon *)pwf);
}

static long config_waveform_command(dbCommon *pxx,
                                    int *option,
                                    uint8_t *buf,
                                    int *len,
                                    void *device,
                                    int transaction_id
                                    )
{
    waveformRecord *pwf = (waveformRecord *)pxx;

    return yew_config_command(buf,
                              len,
                              0, // not used in yew_config_command
                              0, // not used in yew_config_command
                              pwf->nelm,
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
    printf("\n%s: %s %s\n", __FILE__, __func__, pxx->name);

    waveformRecord *pwf = (waveformRecord *)pxx;
    YEW_PLC *d = device;

    long ret = yew_parse_response(buf,
                                  len,
                                  pwf->bptr,
                                  pwf->ftvl, // This has nothing to do with options such as \&F or \&L
                                  pwf->nelm,
                                  option,
                                  d
                                  );

    switch (ret) {
    case NOT_DONE:
        pwf->nord = d->noff;
        // why we don't have break here?
    case 0:
        pwf->nord = pwf->nelm;
    default:
        ;
    }

    return ret;
}
