/* devBiYewPlc.c */
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

#include <biRecord.h>

//
// Binary input (command/response IO)
//
static long init_bi_record(biRecord *);
static long read_bi(biRecord *);
static long config_bi_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_bi_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devBiYewPlc = {
    5,
    NULL,
    netDevInit,
    init_bi_record,
    NULL,
    read_bi
};

epicsExportAddress(dset, devBiYewPlc);

static long init_bi_record(biRecord *prec)
{
    prec->mask = 1;

    YEW_PLC *d = yew_calloc(0, 0, 0, kBit);
    long ret = netDevInitXxRecord((dbCommon *)prec,
                                  &prec->inp,
                                  MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                                  d,
                                  yew_parse_link,
                                  config_bi_command,
                                  parse_bi_response
                                  );

    if (0) {
    } else if (d->conv == kSHORT) {
    //} else if (d->conv == kUSHORT) {
    //} else if (d->conv == kLONG) {
    //} else if (d->conv == kULONG) {
    //} else if (d->conv == kFLOAT) {
    //} else if (d->conv == kDOUBLE) {
    //} else if (d->conv == kBCD) {
    } else {
        errlogPrintf("%s: unsupported conversion \"&%s\" for %s\n", __func__, convstr[d->conv], prec->name);
        prec->pact = 1;
        return -1;
    }

    return ret;
}

static long read_bi(biRecord *prec)
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

static long config_bi_command(dbCommon *pxx,
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

    return yew_config_command(buf,
                              len,
                              0, // not used in yew_config_command
                              0, // not used in yew_config_command
                              1,
                              option,
                              device
                              );
}

static long parse_bi_response(dbCommon *pxx,
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

    biRecord *prec = (biRecord *)pxx;

    return yew_parse_response(buf,
                              len,
                              &prec->rval,
                              DBF_ULONG,
                              1,
                              option,
                              device
                              );
}
