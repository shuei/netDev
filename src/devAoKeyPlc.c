/* devAoKeyPlc.c */
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

FLOATDSET devAoKeyPlc = {
    6,
    NULL,
    netDevInit,
    init_ao_record,
    NULL,
    write_ao,
    ao_linear_convert
};

epicsExportAddress(dset, devAoKeyPlc);

static long init_ao_record(aoRecord *pao)
{
    if (pao->linr == menuConvertLINEAR) {
        pao->eslo = (pao->eguf - pao->egul) / 0xFFFF;
        pao->roff = 0;
    }

    return netDevInitXxRecord((dbCommon *)pao,
                              &pao->out,
                              MPF_WRITE | KEY_GET_PROTO | DEFAULT_TIMEOUT,
                              key_calloc(0, KEY_CMND_WRE),
                              key_parse_link,
                              config_ao_command,
                              parse_ao_response
                              );
}

static long write_ao(aoRecord *pao)
{
    return netDevReadWriteXx((dbCommon *)pao);
}

static long ao_linear_convert(aoRecord *pao, int after)
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

static long config_ao_command(dbCommon *pxx,
                              int *option,
                              uint8_t *buf,
                              int *len,
                              void *device,
                              int transaction_id
                              )
{
    aoRecord *pao = (aoRecord *)pxx;
    KEY_PLC *d = device;
    int16_t val = pao->rval;
    return key_config_command(buf,
                              len,
                              &val,
                              DBF_SHORT,
                              1,
                              option,
                              d
                              );
}

static long parse_ao_response(dbCommon *pxx,
                              int *option,
                              uint8_t *buf,
                              int *len,
                              void *device,
                              int transaction_id
                              )
{
    aoRecord *pao = (aoRecord *)pxx;
    KEY_PLC *d = device;

    return key_parse_response(buf,
                              len,
                              &pao->rval, // not used in key_parse_response
                              DBF_LONG,   // not used in key_parse_response
                              1,
                              option,
                              d
                              );
}
