/* devAoYewPlc.c */
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

#include <aoRecord.h>
#include <menuConvert.h>

//
// Analog output (command/response IO)
//
static long init_ao_record(aoRecord *);
static long write_ao(aoRecord *);
static long ao_linear_convert(aoRecord *, int);
static long config_ao_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_ao_response(dbCommon *, int *, uint8_t *, int *, void *, int);

FLOATDSET devAoYewPlc = {
    6,
    NULL,
    netDevInit,
    init_ao_record,
    NULL,
    write_ao,
    ao_linear_convert
};

epicsExportAddress(dset, devAoYewPlc);

static long init_ao_record(aoRecord *prec)
{
    if (prec->linr == menuConvertLINEAR) {
        prec->eslo = (prec->eguf - prec->egul) / 0xFFFF;
        prec->roff = 0;
    }

    return netDevInitXxRecord((dbCommon *)prec,
                              &prec->out,
                              MPF_WRITE | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                              yew_calloc(0, 0, 0, kWord),
                              yew_parse_link,
                              config_ao_command,
                              parse_ao_response
                              );
}

static long write_ao(aoRecord *prec)
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

static long ao_linear_convert(aoRecord *prec, int after)
{
    if (!after) {
        return OK;
    }

    if (prec->linr == menuConvertLINEAR) {
        prec->eslo = (prec->eguf - prec->egul) / 0xFFFF;
        prec->roff = 0;
    }

    return OK;
}

static long config_ao_command(dbCommon *pxx,
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

    aoRecord *prec = (aoRecord *)pxx;
    YEW_PLC *d = device;

    if (0) {
        //
    } else if (d->flag == 'D') {
        double val = prec->val;
        // todo : consider ASLO and AOFF field

        //prec->udf = isnan(val);
        return yew_config_command(buf,
                                  len,
                                  &val,
                                  DBF_DOUBLE,
                                  1,
                                  option,
                                  d
                                  );
    } else if (d->flag == 'F') {
        float val = prec->val;
        // todo : consider ASLO and AOFF field

        //prec->udf = isnan(val);
        return yew_config_command(buf,
                                  len,
                                  &val,
                                  DBF_FLOAT,
                                  1,
                                  option,
                                  d
                                  );
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

static long parse_ao_response(dbCommon *pxx,
                              int *option,
                              uint8_t *buf,
                              int *len,
                              void *device,
                              int transaction_id
                              )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %s %s\n", __FILE__, __func__, pxx->name);
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
