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

/***************************************************************
 * Analog output (command/response IO)
 ***************************************************************/
LOCAL long init_ao_record(aoRecord *);
LOCAL long write_ao(aoRecord *);
LOCAL long ao_linear_convert(aoRecord *, int);
LOCAL long config_ao_command(dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_ao_response(dbCommon *, int *, uint8_t *, int *, void *, int);

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

LOCAL long init_ao_record(aoRecord *pao)
{
    if (pao->linr == menuConvertLINEAR) {
        pao->eslo = (pao->eguf - pao->egul) / 0xFFFF;
        pao->roff = 0;
    }

    return netDevInitXxRecord((dbCommon *)pao,
                              &pao->out,
                              MPF_WRITE | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                              yew_calloc(0, 0, 0, 2),
                              yew_parse_link,
                              config_ao_command,
                              parse_ao_response
                              );
}

LOCAL long write_ao(aoRecord *pao)
{
    return netDevReadWriteXx((dbCommon *)pao);
}

LOCAL long ao_linear_convert(aoRecord *pao, int after)
{
    if (!after) {
        return OK;
    }

    if (pao->linr == menuConvertLINEAR) {
        pao->eslo = (pao->eguf - pao->egul) / 0xFFFF;
        pao->roff = 0;
    }

    return OK;
}

LOCAL long config_ao_command(dbCommon *pxx,
                             int *option,
                             uint8_t *buf,
                             int *len,
                             void *device,
                             int transaction_id
                             )
{
    aoRecord *pao = (aoRecord *)pxx;
    YEW_PLC *d = (YEW_PLC *)device;

    if (d->flag == 'F') {
        float fval = pao->val;
        // todo : consider ASLO and AOFF field

        void *tmp = &fval;
        int32_t lval = *(int32_t *)tmp;
        int16_t val[2] = { lval >>  0,
                           lval >> 16, };
        //pao->udf = isnan(fval);
        return yew_config_command(buf,
                                  len,
                                  &val[0],
                                  DBF_SHORT,
                                  2,
                                  option,
                                  d
                                  );
    } else if (d->flag == 'L') {
        int16_t val[2] = { pao->rval >>  0,
                           pao->rval >> 16, };
        return yew_config_command(buf,
                                  len,
                                  &val[0],
                                  DBF_SHORT,
                                  2,
                                  option,
                                  d
                                  );
    } else {
        int16_t val = pao->rval;
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

LOCAL long parse_ao_response(dbCommon *pxx,
                             int *option,
                             uint8_t *buf,
                             int *len,
                             void *device,
                             int transaction_id
                             )
{
    aoRecord *pao = (aoRecord *)pxx;
    YEW_PLC *d = (YEW_PLC *)device;

    return yew_parse_response(buf,
                              len,
                              &pao->rval, // not used in yew_parse_response
                              DBF_LONG,   // not used in yew_parse_response
                              (d->flag == 'L' || d->flag == 'F')? 2:1,
                              option,
                              d
                              );
}
