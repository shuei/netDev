/* devMbbiYewPlc.c */
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

#include <mbbiRecord.h>

/***************************************************************
 * Mult-bit binary input (command/response IO)
 ***************************************************************/
LOCAL long init_mbbi_record(struct mbbiRecord *);
LOCAL long read_mbbi(struct mbbiRecord *);
LOCAL long config_mbbi_command(struct dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_mbbi_response(struct dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devMbbiYewPlc = {
    5,
    NULL,
    netDevInit,
    init_mbbi_record,
    NULL,
    read_mbbi
};

epicsExportAddress(dset, devMbbiYewPlc);

LOCAL long init_mbbi_record(struct mbbiRecord *prec)
{
    long status = netDevInitXxRecord((struct dbCommon *) prec,
                                     &prec->inp,
                                     MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                                     yew_calloc(0, 0, 0, 2),
                                     yew_parse_link,
                                     config_mbbi_command,
                                     parse_mbbi_response
                                     );

    prec->nobt = 16;
    prec->mask = 0xFFFF;
    prec->shft = 0;

    return status;
}

LOCAL long read_mbbi(struct mbbiRecord *prec)
{
    return netDevReadWriteXx((struct dbCommon *) prec);
}

LOCAL long config_mbbi_command(struct dbCommon *pxx,
                                     int *option,
                                     uint8_t *buf,
                                     int *len,
                                     void *device,
                                     int transaction_id
                                     )
{
    struct mbbiRecord *prec = (struct mbbiRecord *)pxx;
    YEW_PLC *d = (YEW_PLC *) device;

    return yew_config_command(buf,
                              len,
                              &prec->rval, // not used in yew_config_command
                              DBF_ULONG,   // not used in yew_config_command
                              1,
                              option,
                              d
                              );
}

LOCAL long parse_mbbi_response(struct dbCommon *pxx,
                                     int *option,
                                     uint8_t *buf,
                                     int *len,
                                     void *device,
                                     int transaction_id
                                     )
{
    struct mbbiRecord *prec = (struct mbbiRecord *)pxx;
    YEW_PLC *d = (YEW_PLC *) device;

    int16_t val;
    long ret = yew_parse_response(buf,
                                  len,
                                  &val,
                                  DBF_SHORT,
                                  1,
                                  option,
                                  d
                                  );
    prec->rval = val;
    return ret;
}
