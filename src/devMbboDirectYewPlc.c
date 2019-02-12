/* devMbboDirectYewPlc.c */
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

#include        <mbboDirectRecord.h>

#ifndef EPICS_REVISION
#include <epicsVersion.h>
#endif
#include <epicsExport.h>

/***************************************************************
 * Mult-bit binary output (command/response IO)
 ***************************************************************/
LOCAL long init_mbboDirect_record(struct mbboDirectRecord *);
LOCAL long write_mbboDirect(struct mbboDirectRecord *);
LOCAL long config_mbboDirect_command(struct dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_mbboDirect_response(struct dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devMbboDirectYewPlc = {
  5,
  NULL,
  netDevInit,
  init_mbboDirect_record,
  NULL,
  write_mbboDirect
};

epicsExportAddress(dset, devMbboDirectYewPlc);


LOCAL long init_mbboDirect_record(struct mbboDirectRecord *pMbboDirect)
{
  pMbboDirect->nobt = 16;
  pMbboDirect->mask = 0xFFFF;
  pMbboDirect->shft = 0;

  return netDevInitXxRecord(
			    (struct dbCommon *) pMbboDirect,
			    &pMbboDirect->out,
			    MPF_WRITE | YEW_GET_PROTO | DEFAULT_TIMEOUT,
			    yew_calloc(0, 0, 0, 2),
			    yew_parse_link,
			    config_mbboDirect_command,
			    parse_mbboDirect_response
			    );
}


LOCAL long write_mbboDirect(struct mbboDirectRecord *pMbboDirect)
{
  return netDevReadWriteXx((struct dbCommon *) pMbboDirect);
}


LOCAL long config_mbboDirect_command(
				     struct dbCommon *pxx,
				     int *option,
				     uint8_t *buf,
				     int *len,
				     void *device,
				     int transaction_id
				     )
{
  struct mbboDirectRecord *pmbboDirect = (struct mbboDirectRecord *)pxx;

  return yew_config_command(
			    buf,
			    len,
			    &pmbboDirect->rval,
			    DBF_ULONG,
			    1,
			    option,
			    (YEW_PLC *) device
			    );
} 


LOCAL long parse_mbboDirect_response(
				     struct dbCommon *pxx,
				     int *option,
				     uint8_t *buf,
				     int *len,
				     void *device,
				     int transaction_id
				     )
{
  struct mbboDirectRecord *pmbboDirect = (struct mbboDirectRecord *)pxx;

  return yew_parse_response(
			    buf,
			    len,
			    &pmbboDirect->rval,
			    DBF_ULONG,
			    1,
			    option,
			    (YEW_PLC *) device
			    );
}


