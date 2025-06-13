/* devLongoutYewPlc.c */
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

#include <longoutRecord.h>

//
// Long output (command/respons IO)
//
static long init_longout_record(longoutRecord *);
static long write_longout(longoutRecord *);
static long config_longout_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_longout_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devLoYewPlc = {
    5,
    NULL,
    netDevInit,
    init_longout_record,
    NULL,
    write_longout
};

epicsExportAddress(dset, devLoYewPlc);

static long init_longout_record(longoutRecord *prec)
{
    YEW_PLC *d = yew_calloc(0, 0, 0, kWord);
    long ret = netDevInitXxRecord((dbCommon *)prec,
                                  &prec->out,
                                  MPF_WRITE | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                                  d,
                                  yew_parse_link,
                                  config_longout_command,
                                  parse_longout_response
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

static long write_longout(longoutRecord *prec)
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

static long config_longout_command(dbCommon *pxx,
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

    longoutRecord *prec = (longoutRecord *)pxx;
    YEW_PLC *d = device;

    if (0) {
        //
    } else if (d->conv == kLONG) {
        int32_t val = prec->val;
        return yew_config_command(buf,
                                  len,
                                  &val,
                                  DBF_LONG,
                                  1,
                                  option,
                                  d
                                  );
    //} else if (d->conv == kUSHORT) { // do we need this?
    } else if (d->conv == kBCD) {
        int16_t val = netDevInt2Bcd(prec->val, prec);
        return yew_config_command(buf,
                                  len,
                                  &val,
                                  DBF_SHORT,
                                  1,
                                  option,
                                  d
                                  );
    } else {//(d->conv == kSHORT || d->conv == kUSHORT)
        int16_t val = prec->val;
        return yew_config_command(buf,
                                  len,
                                  &val,
                                  DBF_SHORT,
                                  1,
                                  option,
                                  d
                                  );
    }
}

static long parse_longout_response(dbCommon *pxx,
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

    return yew_parse_response(buf,
                              len,
                              0, // not used in yew_parse_response
                              0, // not used in yew_parse_response
                              1,
                              option,
                              device
                              );
}
