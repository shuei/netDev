/* devMbboYewPlc.c */
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

#include <mbboRecord.h>

//
// Mult-bit binary output (command/response IO)
//
static long init_mbbo_record(mbboRecord *);
static long write_mbbo(mbboRecord *);
static long config_mbbo_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_mbbo_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devMbboYewPlc = {
    5,
    NULL,
    netDevInit,
    init_mbbo_record,
    NULL,
    write_mbbo
};

epicsExportAddress(dset, devMbboYewPlc);

static long init_mbbo_record(mbboRecord *prec)
{
    YEW_PLC *d = yew_calloc(0, 0, 0, kWord);
    long ret = netDevInitXxRecord((dbCommon *)prec,
                                  &prec->out,
                                  MPF_WRITE | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                                  d,
                                  yew_parse_link,
                                  config_mbbo_command,
                                  parse_mbbo_response
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

    if (ret != 0) {
        return ret;
    }
    return 2; // no conversion
}

static long write_mbbo(mbboRecord *prec)
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

static long config_mbbo_command(dbCommon *pxx,
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

    mbboRecord *prec = (mbboRecord *)pxx;
    YEW_PLC *d = device;

    if (0) {
        //
    } else if (d->conv == kLONG) {
        int32_t val = prec->rval;
        return yew_config_command(buf,
                                  len,
                                  &val,
                                  DBF_LONG,
                                  1,
                                  option,
                                  d
                                  );
    } else if (d->conv == kUSHORT) {
        uint16_t val = prec->rval;
        return yew_config_command(buf,
                                  len,
                                  &val,
                                  DBF_USHORT,
                                  1,
                                  option,
                                  d
                                  );
    } else {//(d->conv == kSHORT)
        int16_t val = prec->rval;
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

static long parse_mbbo_response(dbCommon *pxx,
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
