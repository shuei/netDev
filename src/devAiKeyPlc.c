/* devAiKeyPlc.c */
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

#include <aiRecord.h>
#include <menuConvert.h>

//
// Analog input (command/response IO)
//
LOCAL long init_ai_record(aiRecord *);
LOCAL long read_ai(aiRecord *);
LOCAL long ai_linear_convert(aiRecord *, int);
LOCAL long config_ai_command(dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_ai_response(dbCommon *, int *, uint8_t *, int *, void *, int);

FLOATDSET devAiKeyPlc = {
    6,
    NULL,
    netDevInit,
    init_ai_record,
    NULL,
    read_ai,
    ai_linear_convert
};

epicsExportAddress(dset, devAiKeyPlc);

LOCAL long init_ai_record(aiRecord *pai)
{
    if (pai->linr == menuConvertLINEAR) {
        pai->eslo = (pai->eguf - pai->egul) / 0xFFFF;
        pai->roff = 0;
    }

    return netDevInitXxRecord((dbCommon *)pai,
                              &pai->inp,
                              MPF_READ | KEY_GET_PROTO | DEFAULT_TIMEOUT,
                              key_calloc(0, KEY_CMND_RDE),
                              key_parse_link,
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
    KEY_PLC *d = (KEY_PLC *)device;

    return key_config_command(buf,
                              len,
                              &pai->rval, // not used in key_config_command
                              DBF_ULONG,  // not used in key_config_command
                              1,
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
    KEY_PLC *d = (KEY_PLC *)device;

    int16_t val;
    long ret = key_parse_response(buf,
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
