/* devBiKeyPlc.c */
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
 */

#include <biRecord.h>

/***************************************************************
 * Binary input (command/response IO)
 ***************************************************************/
LOCAL long init_bi_record(struct biRecord *);
LOCAL long read_bi(struct biRecord *);
LOCAL long config_bi_command(struct dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_bi_response(struct dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devBiKeyPlc = {
    5,
    NULL,
    netDevInit,
    init_bi_record,
    NULL,
    read_bi
};

epicsExportAddress(dset, devBiKeyPlc);

LOCAL long init_bi_record(struct biRecord *pbi)
{
    pbi->mask = 1;

    return netDevInitXxRecord((struct dbCommon *) pbi,
                              &pbi->inp,
                              MPF_READ | KEY_GET_PROTO | DEFAULT_TIMEOUT,
                              key_calloc(0, KEY_CMND_RD),
                              key_parse_link,
                              config_bi_command,
                              parse_bi_response
                              );
}

LOCAL long read_bi(struct biRecord *pbi)
{
    return netDevReadWriteXx((struct dbCommon *) pbi);
}

LOCAL long config_bi_command(struct dbCommon *pxx,
                             int *option,
                             uint8_t *buf,
                             int *len,
                             void *device,
                             int transaction_id
                             )
{
    struct biRecord *pbi = (struct biRecord *)pxx;

    return key_config_command(buf,
                              len,
                              &pbi->rval,
                              DBF_ULONG,
                              1,
                              option,
                              (KEY_PLC *) device
                              );
}

LOCAL long parse_bi_response(struct dbCommon *pxx,
                             int *option,
                             uint8_t *buf,
                             int *len,
                             void *device,
                             int transaction_id
                             )
{
    struct biRecord *pbi = (struct biRecord *)pxx;

    return key_parse_response(buf,
                              len,
                              &pbi->rval,
                              DBF_ULONG,
                              1,
                              option,
                              (KEY_PLC *) device
                              );
}
