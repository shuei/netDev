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

/***************************************************************
 * Analog input (command/response IO)
 ***************************************************************/
LOCAL long init_ai_record(aiRecord *);
LOCAL long read_ai(aiRecord *);
LOCAL long ai_linear_convert(aiRecord *, int);
LOCAL long config_ai_command(dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_ai_response(dbCommon *, int *, uint8_t *, int *, void *, int);

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

LOCAL long init_ai_record(aiRecord *pai)
{
    if (pai->linr == menuConvertLINEAR) {
        pai->eslo = (pai->eguf - pai->egul) / 0xFFFF;
        pai->roff = 0;
    }

    return netDevInitXxRecord((dbCommon *)pai,
                              &pai->inp,
                              MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                              yew_calloc(0, 0, 0, 2),
                              yew_parse_link,
                              config_ai_command,
                              parse_ai_response
                              );
}

LOCAL long read_ai(aiRecord *pai)
{
    return netDevReadWriteXx((dbCommon *)pai);
}

LOCAL long ai_linear_convert(aiRecord *pai, int after)
{
    if (!after) {
        return OK;
    }

    if (pai->linr == menuConvertLINEAR) {
        pai->eslo = (pai->eguf - pai->egul) / 0xFFFF;
        pai->roff = 0;
    }

    return OK;
}

LOCAL long config_ai_command(dbCommon *pxx,
                             int *option,
                             uint8_t *buf,
                             int *len,
                             void *device,
                             int transaction_id
                             )
{
    aiRecord *pai = (aiRecord *)pxx;
    YEW_PLC *d = (YEW_PLC *)device;

    return yew_config_command(buf,
                              len,
                              &pai->rval, // not used in yew_config_command
                              DBF_LONG,   // not used in yew_config_command
                              (d->flag == 'L' || d->flag == 'F')? 2:1,
                              option,
                              d
                              );
}

LOCAL long parse_ai_response(dbCommon *pxx,
                             int *option,
                             uint8_t *buf,
                             int *len,
                             void *device,
                             int transaction_id
                             )
{
    aiRecord *pai = (aiRecord *)pxx;
    YEW_PLC *d = (YEW_PLC *)device;

    if (d->flag == 'F') {
        uint16_t val[2];
        long ret = yew_parse_response(buf,
                                      len,
                                      &val[0],
                                      DBF_USHORT,
                                      2,
                                      option,
                                      d
                                      );
        uint32_t lval = val[1] << 16 | val[0];
        void *tmp = &lval;
        float *pfloat = tmp;

        if (ret == OK) {
            // todo : consider ASLO and AOFF field
            // todo : consider SMOO field
            pai->val = *pfloat;
            pai->udf = isnan(pai->val);
            ret = 2; /* Don't convert */
        }
        return ret;
    } else if (d->flag == 'L') {
        uint16_t val[2];
        long ret = yew_parse_response(buf,
                                      len,
                                      &val[0],
                                      DBF_USHORT,
                                      2,
                                      option,
                                      d
                                      );
        pai->rval = (val[1]<<16) | (val[0]);
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
        pai->rval = val;
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
        pai->rval = val;
        return ret;
    }
}
