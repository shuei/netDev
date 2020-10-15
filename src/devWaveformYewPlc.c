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

#include        <waveformRecord.h>

//#ifndef EPICS_REVISION
//#include <epicsVersion.h>
//#endif
//#include <epicsExport.h>

/***************************************************************
 * Waveform (command/response IO)
 ***************************************************************/
LOCAL long init_waveform_record(struct waveformRecord *);
LOCAL long read_waveform(struct waveformRecord *);
LOCAL long config_waveform_command(struct dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_waveform_response(struct dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devWfYewPlc = {
    5,
    NULL,
    netDevInit,
    init_waveform_record,
    NULL,
    read_waveform
};

epicsExportAddress(dset, devWfYewPlc);

static uint16_t *u16_val;

LOCAL long init_waveform_record(struct waveformRecord *pwf)
{
    u16_val = (uint16_t *) calloc(2*pwf->nelm, sizeofTypes[DBF_USHORT]);

    if (!u16_val) {
        errlogPrintf("devWaveformYewPlc: calloc failed\n");
        return ERROR;
    }

    return netDevInitXxRecord((struct dbCommon *) pwf,
                              &pwf->inp,
                              MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                              yew_calloc(0, 0, 0, 2),
                              yew_parse_link,
                              config_waveform_command,
                              parse_waveform_response
                              );
}

LOCAL long read_waveform(struct waveformRecord *pwf)
{
    TRANSACTION *t = (TRANSACTION *) pwf->dpvt;
    YEW_PLC *d = (YEW_PLC *) t->device;

    /*
     * make sure that those below are cleared in the event that
     * a multi-step transfer is terminated by an error in the
     * middle of transacton
     */
    d->nleft = 0;
    d->noff = 0;

    return netDevReadWriteXx((struct dbCommon *) pwf);
}

LOCAL long config_waveform_command(struct dbCommon *pxx,
                                   int *option,
                                   uint8_t *buf,
                                   int *len,
                                   void *device,
                                   int transaction_id
                                   )
{
    struct waveformRecord *pwf = (struct waveformRecord *)pxx;

    return yew_config_command(buf,
                              len,
                              pwf->bptr,
                              pwf->ftvl,
                              pwf->nelm,
                              option,
                              (YEW_PLC *) device
                              );
}

LOCAL long parse_waveform_response(struct dbCommon *pxx,
                                   int *option,
                                   uint8_t *buf,
                                   int *len,
                                   void *device,
                                   int transaction_id
                                   )
{
    struct waveformRecord *pwf = (struct waveformRecord *)pxx;
    YEW_PLC *d = (YEW_PLC *) device;

    long ret = yew_parse_response(buf,
                                  len,
                                  pwf->bptr,
                                  pwf->ftvl,
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
