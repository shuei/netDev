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

#include <boRecord.h>

//
// Binary output (command/response IO)
//
static long init_bo_record(boRecord *);
static long write_bo(boRecord *);
static long config_bo_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_bo_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devBoKeyPlc = {
    5,
    NULL,
    netDevInit,
    init_bo_record,
    NULL,
    write_bo
};

epicsExportAddress(dset, devBoKeyPlc);

static long init_bo_record(boRecord *pbo)
{
    pbo->mask = 1;

    return netDevInitXxRecord((dbCommon *)pbo,
                              &pbo->out,
                              MPF_WRITE | KEY_GET_PROTO | DEFAULT_TIMEOUT,
                              key_calloc(0, KEY_CMND_ST_RS),
                              key_parse_link,
                              config_bo_command,
                              parse_bo_response
                              );
}

static long write_bo(boRecord *pbo)
{
    return netDevReadWriteXx((dbCommon *)pbo);
}

static long config_bo_command(dbCommon *pxx,
                              int *option,
                              uint8_t *buf,
                              int *len,
                              void *device,
                              int transaction_id
                              )
{
    boRecord *pbo = (boRecord *)pxx;

    return key_config_command(buf,
                              len,
                              &pbo->rval,
                              DBF_ULONG,
                              1,
                              option,
                              device
                              );
}

static long parse_bo_response(dbCommon *pxx,
                              int *option,
                              uint8_t *buf,
                              int *len,
                              void *device,
                              int transaction_id
                              )
{
    boRecord *pbo = (boRecord *)pxx;

    return key_parse_response(buf,
                              len,
                              &pbo->rval,
                              DBF_ULONG,
                              1,
                              option,
                              device
                              );
}
