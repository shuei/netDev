/* devLongoutKeyPlc.c */
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

#include <longoutRecord.h>

//
// Long output (command/respons IO)
//
LOCAL long init_longout_record(longoutRecord *);
LOCAL long write_longout(longoutRecord *);
LOCAL long config_longout_command(dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_longout_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devLoKeyPlc = {
    5,
    NULL,
    netDevInit,
    init_longout_record,
    NULL,
    write_longout
};

epicsExportAddress(dset, devLoKeyPlc);

LOCAL long init_longout_record(longoutRecord *plongout)
{
    return netDevInitXxRecord((dbCommon *)plongout,
                              &plongout->out,
                              MPF_WRITE | KEY_GET_PROTO | DEFAULT_TIMEOUT,
                              key_calloc(0, KEY_CMND_WRE),
                              key_parse_link,
                              config_longout_command,
                              parse_longout_response
                              );
}


LOCAL long write_longout(longoutRecord *plongout)
{
    return netDevReadWriteXx((dbCommon *)plongout);
}

LOCAL long config_longout_command(dbCommon *pxx,
                                  int *option,
                                  uint8_t *buf,
                                  int *len,
                                  void *device,
                                  int transaction_id
                                  )
{
    longoutRecord *plongout = (longoutRecord *)pxx;
    KEY_PLC *d = (KEY_PLC *)device;

    int16_t val = plongout->val;
    return key_config_command(buf,
                              len,
                              &val,
                              DBF_SHORT,
                              1,
                              option,
                              d
                              );
}

LOCAL long parse_longout_response(dbCommon *pxx,
                                  int *option,
                                  uint8_t *buf,
                                  int *len,
                                  void *device,
                                  int transaction_id
                                  )
{
    longoutRecord *plongout = (longoutRecord *)pxx;
    KEY_PLC *d = (KEY_PLC *)device;

    return key_parse_response(buf,
                              len,
                              &plongout->val, // not used in key_parse_response
                              DBF_LONG,       // not used in key_parse_response
                              1,
                              option,
                              d
                              );
}
