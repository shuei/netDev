/* devMbbiDirectYewPlc.c */
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

#include        <mbbiDirectRecord.h>

#ifndef EPICS_REVISION
#include <epicsVersion.h>
#endif
#include <epicsExport.h>

/***************************************************************
 * Mult-bit binary input (command/response IO)
 ***************************************************************/
LOCAL long init_mbbiDirect_record(struct mbbiDirectRecord *);
LOCAL long read_mbbiDirect(struct mbbiDirectRecord *);
LOCAL long config_mbbiDirect_command(struct dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_mbbiDirect_response(struct dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devMbbiDirectYewPlc = {
  5,
  NULL,
  netDevInit,
  init_mbbiDirect_record,
  NULL,
  read_mbbiDirect
};

epicsExportAddress(dset, devMbbiDirectYewPlc);


LOCAL long init_mbbiDirect_record( struct mbbiDirectRecord *pMbbiDirect )
{
  pMbbiDirect->nobt = 16;
  pMbbiDirect->mask = 0xFFFF;
  pMbbiDirect->shft = 0;

  return netDevInitXxRecord(
			    (struct dbCommon *) pMbbiDirect,
			    &pMbbiDirect->inp,
			    MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
			    yew_calloc(0, 0, 0, 2),
			    yew_parse_link,
			    config_mbbiDirect_command,
			    parse_mbbiDirect_response
			    );
}


LOCAL long read_mbbiDirect(struct mbbiDirectRecord *pMbbiDirect)
{
  return netDevReadWriteXx((struct dbCommon *) pMbbiDirect);
}


LOCAL long config_mbbiDirect_command(
				     struct dbCommon *pxx,
				     int *option,
				     uint8_t *buf,
				     int *len,
				     void *device,
				     int transaction_id
				     )
{
  struct mbbiDirectRecord *pmbbiDirect = (struct mbbiDirectRecord *)pxx;

  return yew_config_command(
			    buf,
			    len,
			    &pmbbiDirect->rval,
			    DBF_ULONG,
			    1,
			    option,
			    (YEW_PLC *) device
			    );
} 


LOCAL long parse_mbbiDirect_response(
				     struct dbCommon *pxx,
				     int *option,
				     uint8_t *buf,
				     int *len,
				     void *device,
				     int transaction_id
				     )
{
  struct mbbiDirectRecord *pmbbiDirect = (struct mbbiDirectRecord *)pxx;

  return yew_parse_response(
			    buf,
			    len,
			    &pmbbiDirect->rval,
			    DBF_ULONG,
			    1,
			    option,
			    (YEW_PLC *) device
			    );
}


