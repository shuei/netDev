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

#include <longinRecord.h>

//
// Long input (command/response IO)
//
static long init_longin_record(longinRecord *);
static long read_longin(longinRecord *);
static long config_longin_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_longin_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devLiKeyPlc = {
    5,
    NULL,
    netDevInit,
    init_longin_record,
    NULL,
    read_longin
};

epicsExportAddress(dset, devLiKeyPlc);

static long init_longin_record(longinRecord *plongin)
{
    return netDevInitXxRecord((dbCommon *)plongin,
                              &plongin->inp,
                              MPF_READ | KEY_GET_PROTO | DEFAULT_TIMEOUT,
                              key_calloc(0, KEY_CMND_RDE),
                              key_parse_link,
                              config_longin_command,
                              parse_longin_response
                              );
}

static long read_longin(longinRecord *plongin)
{
    return netDevReadWriteXx((dbCommon *)plongin);
}

static long config_longin_command(dbCommon *pxx,
                                  int *option,
                                  uint8_t *buf,
                                  int *len,
                                  void *device,
                                  int transaction_id
                                  )
{
    longinRecord *plongin = (longinRecord *)pxx;
    KEY_PLC *d = (KEY_PLC *)device;

    return key_config_command(buf,
                              len,
                              &plongin->val, // not used in key_config_command
                              DBF_ULONG,     // not used in key_config_command
                              1,
                              option,
                              d
                              );
}

static long parse_longin_response(dbCommon *pxx,
                                  int *option,
                                  uint8_t *buf,
                                  int *len,
                                  void *device,
                                  int transaction_id
                                  )
{
    longinRecord *plongin = (longinRecord *)pxx;
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
    plongin->val = val;
    return ret;
}
