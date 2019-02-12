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
/* Modification Log:
 * -----------------
 */

#include        <longoutRecord.h>

#ifndef EPICS_REVISION
#include <epicsVersion.h>
#endif
#include <epicsExport.h>

/***************************************************************
 * Long output (command/respons IO)
 ***************************************************************/
LOCAL long init_longout_record(struct longoutRecord *);
LOCAL long write_longout(struct longoutRecord *);
LOCAL long config_longout_command(struct dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_longout_response(struct dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devLoKeyPlc = {
  5,
  NULL,
  netDevInit,
  init_longout_record,
  NULL,
  write_longout
};

epicsExportAddress(dset, devLoKeyPlc);


LOCAL long init_longout_record(struct longoutRecord *plongout)
{
  return netDevInitXxRecord(
			    (struct dbCommon *) plongout,
			    &plongout->out,
			    MPF_WRITE | KEY_GET_PROTO | DEFAULT_TIMEOUT,
			    key_calloc(0, KEY_CMND_WRE),
			    key_parse_link,
			    config_longout_command,
			    parse_longout_response
			    );
}


LOCAL long write_longout(struct longoutRecord *plongout)
{
  return netDevReadWriteXx((struct dbCommon *) plongout);
}


LOCAL long config_longout_command(
				  struct dbCommon *pxx,
				  int *option,
				  uint8_t *buf,
				  int *len,
				  void *device,
				  int transaction_id
				  )
{
  struct longoutRecord *plongout = (struct longoutRecord *)pxx;
  KEY_PLC *d = (KEY_PLC *) device;

  return key_config_command(
			    buf,
			    len,
			    &plongout->val,
			    DBF_LONG,
			    1,
			    option,
			    d
			    );
}


LOCAL long parse_longout_response(
				  struct dbCommon *pxx,
				  int *option,
				  uint8_t *buf,
				  int *len,
				  void *device,
				  int transaction_id
				  )
{
  struct longoutRecord *plongout = (struct longoutRecord *)pxx;
  KEY_PLC *d = (KEY_PLC *) device;

  return key_parse_response(
			    buf,
			    len,
			    &plongout->val, /* not referenced */
			    DBF_ULONG,      /* not referenced */
			    1,
			    option,
			    d
			    );
}


