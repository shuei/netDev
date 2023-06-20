/* devLonginYewPlc.c */
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

#include <longinRecord.h>

/***************************************************************
 * Long input (command/response IO)
 ***************************************************************/
LOCAL long init_longin_record(longinRecord *);
LOCAL long read_longin(longinRecord *);
LOCAL long config_longin_command(dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_longin_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devLiYewPlc = {
    5,
    NULL,
    netDevInit,
    init_longin_record,
    NULL,
    read_longin
};

epicsExportAddress(dset, devLiYewPlc);

LOCAL long init_longin_record(longinRecord *plongin)
{
    return netDevInitXxRecord((dbCommon *)plongin,
                              &plongin->inp,
                              MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                              yew_calloc(0, 0, 0, 2),
                              yew_parse_link,
                              config_longin_command,
                              parse_longin_response
                              );
}

LOCAL long read_longin(longinRecord *plongin)
{
    return netDevReadWriteXx((dbCommon *)plongin);
}

LOCAL long config_longin_command(dbCommon *pxx,
                                 int *option,
                                 uint8_t *buf,
                                 int *len,
                                 void *device,
                                 int transaction_id
                                 )
{
    longinRecord *plongin = (longinRecord *)pxx;
    YEW_PLC *d = (YEW_PLC *)device;

    return yew_config_command(buf,
                              len,
                              &plongin->val, // not used in yew_config_command
                              DBF_LONG,      // not used in yew_config_command
                              (d->flag == 'L')? 2:1,
                              option,
                              d
                              );
}

LOCAL long parse_longin_response(dbCommon *pxx,
                                 int *option,
                                 uint8_t *buf,
                                 int *len,
                                 void *device,
                                 int transaction_id
                                 )
{
    longinRecord *plongin = (longinRecord *)pxx;
    YEW_PLC *d = (YEW_PLC *)device;

    if (d->flag == 'L') {
        uint16_t val[2];
        long ret = yew_parse_response(buf,
                                      len,
                                      &val[0],
                                      DBF_USHORT,
                                      2,
                                      option,
                                      d
                                      );
        plongin->val = (val[1]<<16) | (val[0]);
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
        plongin->val = val;
        return ret;
    } else if (d->flag == 'B') {
        int16_t val;
        long ret = yew_parse_response(buf,
                                      len,
                                      &val,
                                      DBF_SHORT,
                                      1,
                                      option,
                                      d
                                      );
        plongin->val = netDevBcd2Int(val, plongin);
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
        plongin->val = val;
        return ret;
    }
}
