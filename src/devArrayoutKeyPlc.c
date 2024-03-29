/* devArrayoutKeyPlc.c */
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

#include <arrayoutRecord.h>

//
// Arrayout (command/response IO)
//
static long init_arrayout_record(arrayoutRecord *);
static long write_arrayout(arrayoutRecord *);
static long config_arrayout_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_arrayout_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devAroKeyPlc = {
    5,
    NULL,
    netDevInit,
    init_arrayout_record,
    NULL,
    write_arrayout
};

epicsExportAddress(dset, devAroKeyPlc);

static long init_arrayout_record(arrayoutRecord *pao)
{
    return netDevInitXxRecord((dbCommon *)pao,
                              &pao->out,
                              MPF_WRITE | KEY_GET_PROTO | DEFAULT_TIMEOUT,
                              key_calloc(0, KEY_CMND_WRE),
                              key_parse_link,
                              config_arrayout_command,
                              parse_arrayout_response
                              );
}

static long write_arrayout(arrayoutRecord *pao)
{
    TRANSACTION *t = pao->dpvt;
    KEY_PLC *d = t->device;

    // make sure that those below are cleared in the event that
    // a multi-step transfer is terminated by an error in the
    // middle of transacton
    d->nleft = 0;
    d->noff = 0;

    return netDevReadWriteXx((dbCommon *)pao);
}

static long config_arrayout_command(dbCommon *pxx,
                                    int *option,
                                    uint8_t *buf,
                                    int *len,
                                    void *device,
                                    int transaction_id
                                    )
{
    arrayoutRecord *parrayout = (arrayoutRecord *)pxx;

    return key_config_command(buf,
                              len,
                              parrayout->bptr,
                              parrayout->ftvl,
                              parrayout->nelm,
                              option,
                              device
                              );
}

static long parse_arrayout_response(dbCommon *pxx,
                                    int *option,
                                    uint8_t *buf,
                                    int *len,
                                    void *device,
                                    int transaction_id
                                    )
{
    arrayoutRecord *parrayout = (arrayoutRecord *)pxx;
    KEY_PLC *d = device;

    long ret = key_parse_response(buf,
                                  len,
                                  parrayout->bptr,
                                  parrayout->ftvl,
                                  parrayout->nelm,
                                  option,
                                  d
                                  );

    switch (ret) {
    case NOT_DONE:
        parrayout->nowt = d->noff;
        // why we don't have break here?
    case 0:
        parrayout->nowt = parrayout->nelm;
    default:
        ;
    }

    return ret;
}
