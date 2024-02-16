/* devWaveformKeyPlc.c */
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

#include <waveformRecord.h>

//
// Waveform (command/response IO)
//
static long init_waveform_record(waveformRecord *);
static long read_waveform(waveformRecord *);
static long config_waveform_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_waveform_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devWfKeyPlc = {
    5,
    NULL,
    netDevInit,
    init_waveform_record,
    NULL,
    read_waveform
};

epicsExportAddress(dset, devWfKeyPlc);

static long init_waveform_record(waveformRecord *pwf)
{
    return netDevInitXxRecord((dbCommon *)pwf,
                              &pwf->inp,
                              MPF_READ | KEY_GET_PROTO | DEFAULT_TIMEOUT,
                              key_calloc(0, KEY_CMND_RDE),
                              key_parse_link,
                              config_waveform_command,
                              parse_waveform_response
                              );
}

static long read_waveform(waveformRecord *pwf)
{
    TRANSACTION *t = pwf->dpvt;
    KEY_PLC *d = t->device;

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
    waveformRecord *pwaveform = (waveformRecord *)pxx;

    return key_config_command(buf,
                              len,
                              pwaveform->bptr,
                              pwaveform->ftvl,
                              pwaveform->nelm,
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
    waveformRecord *pwaveform = (waveformRecord *)pxx;
    KEY_PLC *d = device;

    long ret = key_parse_response(buf,
                                  len,
                                  pwaveform->bptr,
                                  pwaveform->ftvl,
                                  pwaveform->nelm,
                                  option,
                                  d
                                  );

    switch (ret) {
    case NOT_DONE:
        pwaveform->nord = d->noff;
        // why we don't have break here?
    case 0:
        pwaveform->nord = pwaveform->nelm;
    default:
        ;
    }

    return ret;
}
