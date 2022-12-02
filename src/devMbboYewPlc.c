/* devMbboYewPlc.c */
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

#include <mbboRecord.h>

/***************************************************************
 * Mult-bit binary output (command/response IO)
 ***************************************************************/
LOCAL long init_mbbo_record(struct mbboRecord *);
LOCAL long write_mbbo(struct mbboRecord *);
LOCAL long config_mbbo_command(struct dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_mbbo_response(struct dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devMbboYewPlc = {
    5,
    NULL,
    netDevInit,
    init_mbbo_record,
    NULL,
    write_mbbo
};

epicsExportAddress(dset, devMbboYewPlc);

LOCAL long init_mbbo_record(struct mbboRecord *prec)
{
    long status = netDevInitXxRecord((struct dbCommon *) prec,
                                     &prec->out,
                                     MPF_WRITE | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                                     yew_calloc(0, 0, 0, 2),
                                     yew_parse_link,
                                     config_mbbo_command,
                                     parse_mbbo_response
                                     );

    prec->nobt = 16;
    prec->mask = 0xFFFF;
    prec->shft = 0;

    if (status != 0) {
        return status;
    }
    return 2; // no conversion
}

LOCAL long write_mbbo(struct mbboRecord *prec)
{
    return netDevReadWriteXx((struct dbCommon *) prec);
}

LOCAL long config_mbbo_command(struct dbCommon *pxx,
                                     int *option,
                                     uint8_t *buf,
                                     int *len,
                                     void *device,
                                     int transaction_id
                                     )
{
    struct mbboRecord *prec = (struct mbboRecord *)pxx;
    YEW_PLC *d = (YEW_PLC *) device;

    if (d->flag == 'U') {
        uint16_t val = prec->rval;
        return yew_config_command(buf,
                                  len,
                                  &val,
                                  DBF_USHORT,
                                  1,
                                  option,
                                  d
                                  );
    } else {
        int16_t val = prec->rval;
        return yew_config_command(buf,
                                  len,
                                  &val,
                                  DBF_SHORT,
                                  1,
                                  option,
                                  d
                                  );
    }
}

LOCAL long parse_mbbo_response(struct dbCommon *pxx,
                                     int *option,
                                     uint8_t *buf,
                                     int *len,
                                     void *device,
                                     int transaction_id
                                     )
{
    struct mbboRecord *prec = (struct mbboRecord *)pxx;

    return yew_parse_response(buf,
                              len,
                              &prec->rval, // not used in yew_parse_response
                              DBF_ULONG,   // not used in yew_parse_response
                              1,
                              option,
                              (YEW_PLC *) device
                              );
}
