/* devMbbiDirectKeyPlc.c */
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

#include <mbbiDirectRecord.h>

/***************************************************************
 * Mult-bit binary input (command/response IO)
 ***************************************************************/
LOCAL long init_mbbiDirect_record(struct mbbiDirectRecord *);
LOCAL long read_mbbiDirect(struct mbbiDirectRecord *);
LOCAL long config_mbbiDirect_command(struct dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_mbbiDirect_response(struct dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devMbbiDirectKeyPlc = {
    5,
    NULL,
    netDevInit,
    init_mbbiDirect_record,
    NULL,
    read_mbbiDirect
};

epicsExportAddress(dset, devMbbiDirectKeyPlc);

LOCAL long init_mbbiDirect_record( struct mbbiDirectRecord *pMbbiDirect )
{
    pMbbiDirect->nobt = 16;
    pMbbiDirect->mask = 0xFFFF;
    pMbbiDirect->shft = 0;

    return netDevInitXxRecord((struct dbCommon *) pMbbiDirect,
                              &pMbbiDirect->inp,
                              MPF_READ | KEY_GET_PROTO | DEFAULT_TIMEOUT,
                              key_calloc(0, KEY_CMND_RDE),
                              key_parse_link,
                              config_mbbiDirect_command,
                              parse_mbbiDirect_response
                              );
}

LOCAL long read_mbbiDirect(struct mbbiDirectRecord *pMbbiDirect)
{
    return netDevReadWriteXx((struct dbCommon *) pMbbiDirect);
}


LOCAL long config_mbbiDirect_command(struct dbCommon *pxx,
                                     int *option,
                                     uint8_t *buf,
                                     int *len,
                                     void *device,
                                     int transaction_id
                                     )
{
    struct mbbiDirectRecord *pmbbiDirect = (struct mbbiDirectRecord *)pxx;

    return key_config_command(buf,
                              len,
                              &pmbbiDirect->rval,
                              DBF_ULONG,
                              1,
                              option,
                              (KEY_PLC *) device
                              );
}

LOCAL long parse_mbbiDirect_response(struct dbCommon *pxx,
                                     int *option,
                                     uint8_t *buf,
                                     int *len,
                                     void *device,
                                     int transaction_id
                                     )
{
    struct mbbiDirectRecord *pmbbiDirect = (struct mbbiDirectRecord *)pxx;

    return key_parse_response(buf,
                              len,
                              &pmbbiDirect->rval,
                              DBF_ULONG,
                              1,
                              option,
                              (KEY_PLC *) device
                              );
}
