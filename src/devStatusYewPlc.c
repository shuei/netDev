/* devStatusYewPlc.c */
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

#include <statusRecord.h>

//
// Status (command/response IO)
//
static long init_status_record(statusRecord *);
static long read_status(statusRecord *);
static long config_status_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_status_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devStatusYewPlc = {
    5,
    NULL,
    netDevInit,
    init_status_record,
    NULL,
    read_status
};

epicsExportAddress(dset, devStatusYewPlc);

static long init_status_record(statusRecord *pst)
{
    return netDevInitXxRecord((dbCommon *)pst,
                              &pst->inp,
                              MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                              yew_calloc(0, 0, 0, 2),
                              yew_parse_link,
                              config_status_command,
                              parse_status_response
                              );
}

static long read_status(statusRecord *pst)
{
    TRANSACTION *t = (TRANSACTION *)pst->dpvt;
    YEW_PLC *d = (YEW_PLC *)t->device;

    // make sure that those below are cleared in the event that
    // a multi-step transfer is terminated by an error in the
    // middle of transacton
    d->nleft = 0;
    d->noff = 0;

    return netDevReadWriteXx((dbCommon *)pst);
}

static long config_status_command(dbCommon *pxx,
                                  int *option,
                                  uint8_t *buf,
                                  int *len,
                                  void *device,
                                  int transaction_id
                                 )
{
    statusRecord *pst = (statusRecord *)pxx;

    return yew_config_command(buf,
                              len,
                              &pst->ch01,
                              DBF_USHORT,
                              pst->nelm,
                              option,
                              (YEW_PLC *)device
                              );
}

static long parse_status_response(dbCommon *pxx,
                                  int *option,
                                  uint8_t *buf,
                                  int *len,
                                  void *device,
                                  int transaction_id
                                  )
{
    statusRecord *pst = (statusRecord *)pxx;
    YEW_PLC *d = (YEW_PLC *)device;
    long ret = yew_parse_response(buf,
                                  len,
                                  &pst->ch01,
                                  DBF_USHORT,
                                  pst->nelm,
                                  option,
                                  d
                                  );

    switch (ret) {
    case NOT_DONE:
        pst->nord = d->noff;
        // why we don't have break here?
    case 0:
        pst->nord = pst->nelm;
    default:
        ;
    }

    return ret;
}
