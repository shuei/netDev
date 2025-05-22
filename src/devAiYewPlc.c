/* devAiYewPlc.c */
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
 * 2006/02/22 jio: added support for long word and floating point data,
 * made all integer data signed
 */

#include <math.h>
#include <aiRecord.h>
#include <menuConvert.h>

//
// Analog input (command/response IO)
//
static long init_ai_record(aiRecord *);
static long read_ai(aiRecord *);
static long ai_linear_convert(aiRecord *, int);
static long config_ai_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_ai_response(dbCommon *, int *, uint8_t *, int *, void *, int);

FLOATDSET devAiYewPlc = {
    6,
    NULL,
    netDevInit,
    init_ai_record,
    NULL,
    read_ai,
    ai_linear_convert
};

epicsExportAddress(dset, devAiYewPlc);

static long init_ai_record(aiRecord *prec)
{
    if (prec->linr == menuConvertLINEAR) {
        prec->eslo = (prec->eguf - prec->egul) / 0xFFFF;
        prec->roff = 0;
    }

    return netDevInitXxRecord((dbCommon *)prec,
                              &prec->inp,
                              MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                              yew_calloc(0, 0, 0, kWord),
                              yew_parse_link,
                              config_ai_command,
                              parse_ai_response
                              );
}

static long read_ai(aiRecord *prec)
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

static long ai_linear_convert(aiRecord *prec, int after)
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

static long config_ai_command(dbCommon *pxx,
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

static long parse_ai_response(dbCommon *pxx,
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

    aiRecord *prec = (aiRecord *)pxx;
    YEW_PLC *d = device;

    if (0) {
        //
    } else if (d->flag == 'D') {
        double val;
        long ret = yew_parse_response(buf,
                                      len,
                                      &val,
                                      DBF_DOUBLE,
                                      1,
                                      option,
                                      d
                                      );

        if (ret == OK) {
            // todo : consider ASLO and AOFF field
            // todo : consider SMOO field
            prec->val = val;
            prec->udf = isnan(val);
            ret = 2; // Don't convert
        }
        return ret;
    } else if (d->flag == 'F') {
        float val;
        long ret = yew_parse_response(buf,
                                      len,
                                      &val,
                                      DBF_FLOAT,
                                      1,
                                      option,
                                      d
                                      );

        if (ret == OK) {
            // todo : consider ASLO and AOFF field
            // todo : consider SMOO field
            prec->val = val;
            prec->udf = isnan(val);
            ret = 2; // Don't convert
        }
        return ret;
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
