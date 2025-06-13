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

static long init_status_record(statusRecord *prec)
{
    YEW_PLC *d = yew_calloc(0, 0, 0, kWord);
    long ret = netDevInitXxRecord((dbCommon *)prec,
                                  &prec->inp,
                                  MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                                  d,
                                  yew_parse_link,
                                  config_status_command,
                                  parse_status_response
                                  );

    if (0) {
    } else if (d->conv == kSHORT) {
    } else if (d->conv == kUSHORT) {
    } else if (d->conv == kLONG) {
    //} else if (d->conv == kULONG) {
    //} else if (d->conv == kFLOAT) {
    //} else if (d->conv == kDOUBLE) {
    } else if (d->conv == kBCD) {
    } else {
        errlogPrintf("%s: unsupported conversion \"&%s\" for %s\n", __func__, convstr[d->conv], prec->name);
        prec->pact = 1;
        return -1;
    }

    return ret;
}

static long read_status(statusRecord *prec)
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

static long config_status_command(dbCommon *pxx,
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

    statusRecord *prec = (statusRecord *)pxx;

    return yew_config_command(buf,
                              len,
                              0, // not used in yew_config_command
                              0, // not used in yew_config_command
                              prec->nelm,
                              option,
                              device
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
    //DEBUG
    if (netDevDebug>0) {
        printf("\n%s: %s %s\n", __FILE__, __func__, pxx->name);
    }

    statusRecord *prec = (statusRecord *)pxx;
    YEW_PLC *d = device;

    long ret = yew_parse_response(buf,
                                  len,
                                  &prec->ch01,
                                  DBF_USHORT,
                                  prec->nelm,
                                  option,
                                  d
                                  );

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
