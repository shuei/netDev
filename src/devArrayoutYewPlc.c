/* devArrayoutYewPlc.c */
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

#include <arrayoutRecord.h>

//
// Arrayout (command/response IO)
//
static long init_arrayout_record(arrayoutRecord *);
static long write_arrayout(arrayoutRecord *);
static long config_arrayout_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_arrayout_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devAroYewPlc = {
    5,
    NULL,
    netDevInit,
    init_arrayout_record,
    NULL,
    write_arrayout
};

epicsExportAddress(dset, devAroYewPlc);

static long init_arrayout_record(arrayoutRecord *paro)
{
    return netDevInitXxRecord((dbCommon *)paro,
                              &paro->out,
                              MPF_WRITE | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                              yew_calloc(0, 0, 0, kWord),
                              yew_parse_link,
                              config_arrayout_command,
                              parse_arrayout_response
                              );
}

static long write_arrayout(arrayoutRecord *paro)
{
    TRANSACTION *t = paro->dpvt;
    YEW_PLC *d = t->device;

    // make sure that those below are cleared in the event that
    // a multi-step transfer is terminated by an error in the
    // middle of transacton
    d->nleft = 0;
    d->noff = 0;

    return netDevReadWriteXx((dbCommon *)paro);
}

static long config_arrayout_command(dbCommon *pxx,
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

    arrayoutRecord *paro = (arrayoutRecord *)pxx;

    return yew_config_command(buf,
                              len,
                              paro->bptr,
                              paro->ftvl, // This has nothing to do with options such as \&F or \&L
                              paro->nelm,
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
    arrayoutRecord *paro = (arrayoutRecord *)pxx;
    YEW_PLC *d = device;

    long ret = yew_parse_response(buf,
                                  len,
                                  0, // not used in yew_parse_response
                                  0, // not used in yew_parse_response
                                  paro->nelm,
                                  option,
                                  d
                                  );

    switch (ret) {
    case NOT_DONE:
        paro->nowt = d->noff;
        // why we don't have break here?
    case 0:
        paro->nowt = paro->nelm;
    default:
        ;
    }

    return ret;
}
