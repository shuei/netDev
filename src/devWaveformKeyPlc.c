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
LOCAL long init_waveform_record(waveformRecord *);
LOCAL long read_waveform(waveformRecord *);
LOCAL long config_waveform_command(dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_waveform_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devWfKeyPlc = {
    5,
    NULL,
    netDevInit,
    init_waveform_record,
    NULL,
    read_waveform
};

epicsExportAddress(dset, devWfKeyPlc);

LOCAL long init_waveform_record(waveformRecord *pwf)
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

LOCAL long read_waveform(waveformRecord *pwf)
{
    TRANSACTION *t = (TRANSACTION *)pwf->dpvt;
    KEY_PLC *d = (KEY_PLC *)t->device;

    // make sure that those below are cleared in the event that
    // a multi-step transfer is terminated by an error in the
    // middle of transacton
    d->nleft = 0;
    d->noff = 0;

    return netDevReadWriteXx((dbCommon *)pwf);
}

LOCAL long config_waveform_command(dbCommon *pxx,
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
                              (KEY_PLC *)device
                              );
}

LOCAL long parse_waveform_response(dbCommon *pxx,
                                   int *option,
                                   uint8_t *buf,
                                   int *len,
                                   void *device,
                                   int transaction_id
                                   )
{
    waveformRecord *pwaveform = (waveformRecord *)pxx;
    KEY_PLC *d = (KEY_PLC *)device;

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
