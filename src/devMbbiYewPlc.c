/* devMbbiYewPlc.c */
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

#include <mbbiRecord.h>

//
// Mult-bit binary input (command/response IO)
//
static long init_mbbi_record(mbbiRecord *);
static long read_mbbi(mbbiRecord *);
static long config_mbbi_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_mbbi_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devMbbiYewPlc = {
    5,
    NULL,
    netDevInit,
    init_mbbi_record,
    NULL,
    read_mbbi
};

epicsExportAddress(dset, devMbbiYewPlc);

static long init_mbbi_record(mbbiRecord *prec)
{
    long status = netDevInitXxRecord((dbCommon *)prec,
                                     &prec->inp,
                                     MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                                     yew_calloc(0, 0, 0, kWord),
                                     yew_parse_link,
                                     config_mbbi_command,
                                     parse_mbbi_response
                                     );

    prec->nobt = 32;
    prec->mask = 0xFFFFFFFF;
    prec->shft = 0;

    return status;
}

static long read_mbbi(mbbiRecord *prec)
{
    return netDevReadWriteXx((dbCommon *)prec);
}

static long config_mbbi_command(dbCommon *pxx,
                                int *option,
                                uint8_t *buf,
                                int *len,
                                void *device,
                                int transaction_id
                                )
{
    return yew_config_command(buf,
                              len,
                              0, // not used in yew_config_command
                              0, // not used in yew_config_command
                              1,
                              option,
                              device
                              );
}

static long parse_mbbi_response(dbCommon *pxx,
                                int *option,
                                uint8_t *buf,
                                int *len,
                                void *device,
                                int transaction_id
                                )
{
    //DEBUG
    printf("\n%s: %s %s\n", __FILE__, __func__, pxx->name);

    mbbiRecord *prec = (mbbiRecord *)pxx;
    YEW_PLC *d = device;

    if (0) {
        //
    } else if (d->flag == 'L') {
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
    } else if (d->flag == 'U') {
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
    } else {
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
