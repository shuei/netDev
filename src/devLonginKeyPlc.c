/* devLonginKeyPlc.c */
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

#include <longinRecord.h>

/***************************************************************
 * Long input (command/response IO)
 ***************************************************************/
LOCAL long init_longin_record(struct longinRecord *);
LOCAL long read_longin(struct longinRecord *);
LOCAL long config_longin_command(struct dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_longin_response(struct dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devLiKeyPlc = {
    5,
    NULL,
    netDevInit,
    init_longin_record,
    NULL,
    read_longin
};

epicsExportAddress(dset, devLiKeyPlc);

LOCAL long init_longin_record(struct longinRecord *plongin)
{
    return netDevInitXxRecord((struct dbCommon *) plongin,
                              &plongin->inp,
                              MPF_READ | KEY_GET_PROTO | DEFAULT_TIMEOUT,
                              key_calloc(0, KEY_CMND_RDE),
                              key_parse_link,
                              config_longin_command,
                              parse_longin_response
                              );
}

LOCAL long read_longin(struct longinRecord *plongin)
{
    return netDevReadWriteXx((struct dbCommon *) plongin);
}

LOCAL long config_longin_command(struct dbCommon *pxx,
                                 int *option,
                                 uint8_t *buf,
                                 int *len,
                                 void *device,
                                 int transaction_id
                                 )
{
    struct longinRecord *plongin = (struct longinRecord *)pxx;
    KEY_PLC *d = (KEY_PLC *) device;

    return key_config_command(buf,
                              len,
                              &plongin->val, // not used in key_config_command
                              DBF_ULONG,     // not used in key_config_command
                              1,
                              option,
                              d
                              );
}

LOCAL long parse_longin_response(struct dbCommon *pxx,
                                 int *option,
                                 uint8_t *buf,
                                 int *len,
                                 void *device,
                                 int transaction_id
                                 )
{
    struct longinRecord *plongin = (struct longinRecord *)pxx;
    KEY_PLC *d = (KEY_PLC *) device;

    int16_t val;
    long ret = key_parse_response(buf,
                                  len,
                                  &val,
                                  DBF_SHORT,
                                  1,
                                  option,
                                  d
                              );
    plongin->val = val;
    return ret;
}
