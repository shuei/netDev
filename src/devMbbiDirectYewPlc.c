/* devMbbiDirectYewPlc.c */
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

#include <mbbiDirectRecord.h>

//
// Mult-bit binary input (command/response IO)
//
static long init_mbbiDirect_record(mbbiDirectRecord *);
static long read_mbbiDirect(mbbiDirectRecord *);
static long config_mbbiDirect_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_mbbiDirect_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devMbbiDirectYewPlc = {
    5,
    NULL,
    netDevInit,
    init_mbbiDirect_record,
    NULL,
    read_mbbiDirect
};

epicsExportAddress(dset, devMbbiDirectYewPlc);

static long init_mbbiDirect_record(mbbiDirectRecord *prec)
{
    YEW_PLC *d = yew_calloc(0, 0, 0, kWord);
    long ret = netDevInitXxRecord((dbCommon *)prec,
                                  &prec->inp,
                                  MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                                  d,
                                  yew_parse_link,
                                  config_mbbiDirect_command,
                                  parse_mbbiDirect_response
                                  );

    if (0) {
    } else if (d->conv == kSHORT) {
        prec->nobt = 16;
        prec->mask = 0xffff;
        prec->shft = 0;
    } else if (d->conv == kUSHORT) {
        prec->nobt = 16;
        prec->mask = 0xffff;
        prec->shft = 0;
    } else if (d->conv == kLONG) {
        prec->nobt = 32;
        prec->mask = 0xffffffff;
        prec->shft = 0;
    //} else if (d->conv == kULONG) {
    //} else if (d->conv == kFLOAT) {
    //} else if (d->conv == kDOUBLE) {
    //} else if (d->conv == kBCD) {
    } else{
        errlogPrintf("%s: unsupported conversion \"&%s\" for %s\n", __func__, convstr[d->conv], prec->name);
        prec->pact = 1;
        return -1;
    }

    return ret;
}

static long read_mbbiDirect(mbbiDirectRecord *prec)
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

static long config_mbbiDirect_command(dbCommon *pxx,
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

static long parse_mbbiDirect_response(dbCommon *pxx,
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

    mbbiDirectRecord *prec = (mbbiDirectRecord *)pxx;
    YEW_PLC *d = device;

    if (0) {
        //
    } else if (d->conv == kLONG) {
        int32_t val;
        long ret = yew_parse_response(buf,
                                      len,
                                      &val,
                                      DBF_LONG,
                                      1,
                                      option,
                                      d
                                      );
        prec->rval = val;
        return ret;
    } else if (d->conv == kUSHORT) {
        uint16_t val;
        long ret = yew_parse_response(buf,
                                      len,
                                      &val,
                                      DBF_USHORT,
                                      1,
                                      option,
                                      d
                                      );
        prec->rval = val;
        return ret;
    } else {//(d->conv == kSHORT)
        int16_t val;
        long ret = yew_parse_response(buf,
                                      len,
                                      &val,
                                      DBF_SHORT,
                                      1,
                                      option,
                                      d
                                      );
        prec->rval = val;
        return ret;
    }
}
