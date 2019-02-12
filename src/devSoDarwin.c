/* devSoDarwin.c */
/****************************************************************************
 *                         COPYRIGHT NOTIFICATION
 *
 * Copyright (c) All rights reserved
 *
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 ****************************************************************************/
/* Author: Jun-ichi Odagiri */
/* Modification Log:
 * -----------------
 */
#include        <stringoutRecord.h>

#ifndef EPICS_REVISION
#include <epicsVersion.h>
#endif
#if EPICS_REVISION == 14 && EPICS_MODIFICATION >= 2
#include <epicsExport.h>
#endif

/***************************************************************
 * String output (command/respons IO)
 ***************************************************************/
LOCAL long init_so_record(struct stringoutRecord *);
LOCAL long write_so(struct stringoutRecord *);
LOCAL long config_so_command(struct dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_so_response(struct dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devSoDarwin = {
  5,
  NULL,
  netDevInit,
  init_so_record,
  NULL,
  write_so
};

#if EPICS_REVISION == 14 && EPICS_MODIFICATION >= 2
epicsExportAddress(dset, devSoDarwin);
#endif


LOCAL long init_so_record(struct stringoutRecord *pso)
{
  LOGMSG("devSoDarwin: init_so_record(\"%s\")\n",
	 pso->name,0,0,0,0,0,0,0,0);

  return netDevInitXxRecord(
			    (struct dbCommon *) pso,
			    &pso->out,
			    MPF_WRITE | MPF_TCP | DARWIN_TIMEOUT,
			    darwin_calloc(),
			    darwin_parse_link,
			    config_so_command,
			    parse_so_response
			    );
}


LOCAL long write_so(struct stringoutRecord *pso)
{
    LOGMSG("devSoDarwin: write_so(\"%s\")\n",
	   pso->name,0,0,0,0,0,0,0,0);

    return netDevReadWriteXx((struct dbCommon *) pso);
}


LOCAL long config_so_command(
			     struct dbCommon *pxx,
			     int *option,
			     uint8_t *buf,
			     int *len,
			     void *device,
			     int transaction_id
			     )
{
    stringoutRecord *pso = (stringoutRecord *) pxx;
    DARWIN *d = (DARWIN *) device;
    char terminator[3] = "\r\n";

    LOGMSG("devSoDarwin: config_so_command(\"%s\")\n",
	   pxx->name,0,0,0,0,0,0,0,0);

    if (*len < strlen(pso->val) + strlen(terminator) + 1)
      {
        errlogPrintf("devDarwin: buffer is running short\n");
        return ERROR;
      }

    sprintf(buf, "%s", pso->val);
    strcat(buf, terminator);
    *len = strlen(buf);

    return 0;
}



LOCAL long parse_so_response(
			     struct dbCommon *pxx,
			     int *option,
			     uint8_t *buf,
			     int *len,
			     void *device,
			     int transaction_id
			     )
{
    int code;

    LOGMSG("devSoDarwin: parse_so_response(0x%08x,0x%08x,0x%08x,%d,0x%08x,%d)\n",
	   pxx,*option,buf,*len,device,transaction_id,0,0,0);

    if (sscanf(buf, "E%d\r\n", &code) != 1)
      {
	errlogPrintf("parse_so_response: failed to read returned error code\n");
        return ERROR;
      }

    if (code)
      {
	errlogPrintf("parse_so_response: error code %d returned\n", code);
        return ERROR;
      }

    return OK;
}