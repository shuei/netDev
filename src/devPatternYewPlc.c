/* devPatternYewPlc.c */
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

#include <patternRecord.h>

//
// Pattern (command/response IO)
//
LOCAL long init_pattern_record(patternRecord *);
LOCAL long read_pattern(patternRecord *);
LOCAL long config_pattern_command(dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_pattern_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devPtnYewPlc = {
    5,
    NULL,
    netDevInit,
    init_pattern_record,
    NULL,
    read_pattern
};

epicsExportAddress(dset, devPtnYewPlc);

LOCAL long init_pattern_record(patternRecord *pptn)
{
    return netDevInitXxRecord((dbCommon *)pptn,
                              &pptn->inp,
                              MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                              yew_calloc(0, 0, 0, 2),
                              yew_parse_link,
                              config_pattern_command,
                              parse_pattern_response
                              );
}

LOCAL long read_pattern(patternRecord *pptn)
{
    TRANSACTION *t = (TRANSACTION *)pptn->dpvt;
    YEW_PLC *d = (YEW_PLC *)t->device;


    // make sure that those below are cleared in the event that
    // a multi-step transfer is terminated by an error in the
    // middle of transacton
    d->nleft = 0;
    d->noff = 0;

    return netDevReadWriteXx((dbCommon *)pptn);
}

LOCAL long config_pattern_command(dbCommon *pxx,
                                  int *option,
                                  uint8_t *buf,
                                  int *len,
                                  void *device,
                                  int transaction_id
                                  )
{
    patternRecord *pptn = (patternRecord *)pxx;

    return yew_config_command(buf,
                              len,
                              pptn->rptr,
                              pptn->ftvl,
                              pptn->nelm,
                              option,
                              (YEW_PLC *)device
                              );
}

LOCAL long parse_pattern_response(dbCommon *pxx,
                                  int *option,
                                  uint8_t *buf,
                                  int *len,
                                  void *device,
                                  int transaction_id
                                  )
{
    patternRecord *pptn = (patternRecord *)pxx;
    YEW_PLC *d = (YEW_PLC *)device;

    long ret = yew_parse_response(buf,
                                  len,
                                  pptn->rptr,
                                  pptn->ftvl,
                                  pptn->nelm,
                                  option,
                                  d
                                  );

    switch (ret) {
    case NOT_DONE:
        pptn->nord = d->noff;
        // why we don't have break here?
    case 0:
        pptn->nord = pptn->nelm;
    default:
        ;
    }

    return ret;
}
