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

#include        <longinRecord.h>

#ifndef EPICS_REVISION
#include <epicsVersion.h>
#endif
#include <epicsExport.h>

/***************************************************************
 * Long input (command/response IO)
 ***************************************************************/
LOCAL long init_longin_record(struct longinRecord *);
LOCAL long read_longin(struct longinRecord *);
LOCAL long config_longin_command(struct dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_longin_response(struct dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devLiYewPlc = {
    5,
    NULL,
    netDevInit,
    init_longin_record,
    NULL,
    read_longin
};

epicsExportAddress(dset, devLiYewPlc);

LOCAL long init_longin_record(struct longinRecord *plongin)
{
    return netDevInitXxRecord((struct dbCommon *) plongin,
                              &plongin->inp,
                              MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                              yew_calloc(0, 0, 0, 2),
                              yew_parse_link,
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
    YEW_PLC *d = (YEW_PLC *) device;

    return yew_config_command(buf,
                              len,
                              &plongin->val, /* not referenced */
                              DBF_LONG,      /* not referenced */
                              (d->dword)? 2:1,
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
    YEW_PLC *d = (YEW_PLC *) device;
    long ret;

    if (d->dword) {
        uint16_t u16_val[2];

        ret = yew_parse_response(buf,
                                 len,
                                 &u16_val[0],
                                 DBF_USHORT,
                                 2,
                                 option,
                                 d
                                 );

        plongin->val = u16_val[1] << 16 | u16_val[0];
    } else {
        ret = yew_parse_response(buf,
                                 len,
                                 &plongin->val,
                                 DBF_LONG,
                                 1,
                                 option,
                                 d
                                 );
    }

    return ret;
}
