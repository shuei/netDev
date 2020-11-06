/* devBoKeyPlc.c */
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

#include <epicsExport.h>
#include <boRecord.h>

/***************************************************************
 * Binary output (command/response IO)
 ***************************************************************/
LOCAL long init_bo_record(struct boRecord *);
LOCAL long write_bo(struct boRecord *);
LOCAL long config_bo_command(struct dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_bo_response(struct dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devBoKeyPlc = {
    5,
    NULL,
    netDevInit,
    init_bo_record,
    NULL,
    write_bo
};

epicsExportAddress(dset, devBoKeyPlc);

LOCAL long init_bo_record(struct boRecord *pbo)
{
    pbo->mask = 1;

    return netDevInitXxRecord((struct dbCommon *) pbo,
                              &pbo->out,
                              MPF_WRITE | KEY_GET_PROTO | DEFAULT_TIMEOUT,
                              key_calloc(0, KEY_CMND_ST_RS),
                              key_parse_link,
                              config_bo_command,
                              parse_bo_response
                              );
}

LOCAL long write_bo(struct boRecord *pbo)
{
    return netDevReadWriteXx((struct dbCommon *) pbo);
}


LOCAL long config_bo_command(struct dbCommon *pxx,
                             int *option,
                             uint8_t *buf,
                             int *len,
                             void *device,
                             int transaction_id
                             )
{
    struct boRecord *pbo = (struct boRecord *)pxx;

    return key_config_command(buf,
                              len,
                              &pbo->rval,
                              DBF_ULONG,
                              1,
                              option,
                              (KEY_PLC *) device
                              );
}

LOCAL long parse_bo_response(struct dbCommon *pxx,
                             int *option,
                             uint8_t *buf,
                             int *len,
                             void *device,
                             int transaction_id
                             )
{
    struct boRecord *pbo = (struct boRecord *)pxx;

    return key_parse_response(buf,
                              len,
                              &pbo->rval,
                              DBF_ULONG,
                              1,
                              option,
                              (KEY_PLC *) device
                              );
}
