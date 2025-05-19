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

//
// Long input (command/response IO)
//
static long init_longin_record(longinRecord *);
static long read_longin(longinRecord *);
static long config_longin_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_longin_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devLiYewPlc = {
    5,
    NULL,
    netDevInit,
    init_longin_record,
    NULL,
    read_longin
};

epicsExportAddress(dset, devLiYewPlc);

static long init_longin_record(longinRecord *plongin)
{
    return netDevInitXxRecord((dbCommon *)plongin,
                              &plongin->inp,
                              MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                              yew_calloc(0, 0, 0, kWord),
                              yew_parse_link,
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
    //DEBUG
    if (netDevDebug>0) {
        printf("\n%s: %s %s\n", __FILE__, __func__, pxx->name);
    }

    return yew_config_command(buf,
                              len,
                              0, // not used in yew_config_command
                              0, // not used in yew_config_command
                              1,
                              option,
                              device
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
    //DEBUG
    if (netDevDebug>0) {
        printf("\n%s: %s %s\n", __FILE__, __func__, pxx->name);
    }

    longinRecord *plongin = (longinRecord *)pxx;
    YEW_PLC *d = device;

    if (0) {
        //
    } else if (d->flag == 'L') {
        int32_t val;
        long ret = yew_parse_response(buf,
                                      len,
                                      &val,
                                      DBF_LONG,
                                      1,
                                      option,
                                      d
                                      );
        plongin->val = val;
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
