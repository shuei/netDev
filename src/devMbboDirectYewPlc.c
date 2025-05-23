/* devMbboDirectYewPlc.c */
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

#include <mbboDirectRecord.h>

//
// Mult-bit binary output (command/response IO)
//
static long init_mbboDirect_record(mbboDirectRecord *);
static long write_mbboDirect(mbboDirectRecord *);
static long config_mbboDirect_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_mbboDirect_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devMbboDirectYewPlc = {
    5,
    NULL,
    netDevInit,
    init_mbboDirect_record,
    NULL,
    write_mbboDirect
};

epicsExportAddress(dset, devMbboDirectYewPlc);

static long init_mbboDirect_record(mbboDirectRecord *prec)
{
    YEW_PLC *d = yew_calloc(0, 0, 0, kWord);
    long status = netDevInitXxRecord((dbCommon *)prec,
                                     &prec->out,
                                     MPF_WRITE | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                                     d,
                                     yew_parse_link,
                                     config_mbboDirect_command,
                                     parse_mbboDirect_response
                                     );

    if (0) {
        //
    } else if (d->flag == 'L') {
        prec->nobt = 32;
        prec->mask = 0xffffffff;
        prec->shft = 0;
    } else {
        prec->nobt = 16;
        prec->mask = 0xffff;
        prec->shft = 0;
    }

    if (status != 0) {
        return status;
    }
    return 2; // no conversion
}

static long write_mbboDirect(mbboDirectRecord *prec)
{
    return netDevReadWriteXx((dbCommon *)prec);
}

static long config_mbboDirect_command(dbCommon *pxx,
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

    mbboDirectRecord *prec = (mbboDirectRecord *)pxx;
    YEW_PLC *d = device;

    if (0) {
        //
    } else if (d->flag == 'L') {
        int32_t val = prec->rval;
        return yew_config_command(buf,
                                  len,
                                  &val,
                                  DBF_LONG,
                                  1,
                                  option,
                                  d
                                  );
    } else if (d->flag == 'U') {
        uint16_t val = prec->rval;
        return yew_config_command(buf,
                                  len,
                                  &val,
                                  DBF_USHORT,
                                  1,
                                  option,
                                  d
                                  );
    } else {
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

static long parse_mbboDirect_response(dbCommon *pxx,
                                      int *option,
                                      uint8_t *buf,
                                      int *len,
                                      void *device,
                                      int transaction_id
                                      )
{
    return yew_parse_response(buf,
                              len,
                              0, // not used in yew_parse_response
                              0, // not used in yew_parse_response
                              1,
                              option,
                              device
                              );
}
