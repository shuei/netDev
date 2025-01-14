/* channelsRecord.c */
/****************************************************************************
 *                         COPYRIGHT NOTIFICATION
 *
 * Copyright (c) All rights reserved
 *
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 * This work is based on waveformRecord.c
 ****************************************************************************/
/* Current Author: Jun-ichi Odagiri (jun-ichi.odagiri@kek.jp, KEK) */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <alarm.h>
#include <dbAccess.h>
#include <dbDefs.h>
#include <dbEvent.h>
#include <devSup.h>
#include <epicsExport.h>
#include <errMdef.h>
#include <recGbl.h>
#include <recSup.h>
#include <special.h>

#define GEN_SIZE_OFFSET
#include "channelsRecord.h"
#undef  GEN_SIZE_OFFSET

// Create RSET - Record Support Entry Table
#define report NULL
#define initialize NULL
static long init_record(dbCommon *, int);
static long process(dbCommon *);
#define special NULL
#define get_value NULL
static long cvt_dbaddr();
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
static long get_precision ();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

rset channelsRSET = {
    RSETNUMBER,
    report,
    initialize,
    init_record,
    process,
    special,
    get_value,
    cvt_dbaddr,
    get_array_info,
    put_array_info,
    get_units,
    get_precision,
    get_enum_str,
    get_enum_strs,
    put_enum_str,
    get_graphic_double,
    get_control_double,
    get_alarm_double
};

epicsExportAddress(rset, channelsRSET);

#define ALARM_MSG_SIZE  MAX_STRING_SIZE
#define ENG_UNIT_SIZE   8

// channels dset
typedef struct channels_dset {
    long       number;
    DEVSUPFUN  dev_report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record; // returns: (-1,0)=>(failure,success)
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_channels;
} channels_dset;

static void checkAlarms(channelsRecord *pchan);
static void monitor(channelsRecord *pchan);

static long init_record(dbCommon *precord, int pass)
{
    if (pass == 0) {
        return 0;
    }

    channelsRecord *pchan = (channelsRecord *)precord;
    channels_dset  *pdset = (channels_dset *)(pchan->dset);

    if (!pdset) {
        recGblRecordError(S_dev_noDSET, pchan, "channels: No DSET");
        return S_dev_noDSET;
    }

    // must have read_channels function defined
    if ((pdset->number < 5) || (pdset->read_channels == NULL)) {
        recGblRecordError(S_dev_missingSup, pchan, "channels: Bad DSET");
        return S_dev_missingSup;
    }

    if (pdset->init_record) {
        long status = (*pdset->init_record)(pchan);
        if (status) {
            return status;
        }
    }

    return 0;
}

static long process(dbCommon *precord)
{
    channelsRecord *pchan = (channelsRecord *)precord;
    channels_dset  *pdset = (channels_dset *)(pchan->dset);
    unsigned char  pact   = pchan->pact;

    if ((pdset == NULL) || (pdset->read_channels == NULL)) {
        pchan->pact=TRUE;
        recGblRecordError(S_dev_missingSup, pchan, "read_channels");
        return S_dev_missingSup;
    }

    // pact must not be set until after calling device support
    long status = (*pdset->read_channels)(pchan);

    // check if device support set pact
    if (!pact && pchan->pact) {
        return 0;
    }

    pchan->pact = TRUE;

    recGblGetTimeStamp(pchan);
    // check for alarms
    checkAlarms(pchan);
    // check event list
    monitor(pchan);
    // process the forward scan link record
    recGblFwdLink(pchan);

    pchan->pact=FALSE;

    return status;
}

static long get_precision(DBADDR *paddr, long *precision)
{
    channelsRecord *pchan = (channelsRecord *)paddr->precord;

    *precision = pchan->prec;
    if (paddr->pfield == &pchan->ch01) {
        return 0;
    }
    recGblGetPrec(paddr,precision);
    return 0;
}

static void checkAlarms(channelsRecord *pchan)
{
    return;
}

static void monitor(channelsRecord *pchan)
{
    double *pData = (double *)&pchan->ch01;
    char (*pAlrm)[MAX_STRING_SIZE] = (char (*)[MAX_STRING_SIZE])&pchan->al01;
    char (*pUnit)[8] = (char (*)[8])&pchan->eu01;
    short *pPos = (short *)&pchan->pp01;

    db_post_events(pchan, &pchan->time, DBE_VALUE|DBE_LOG);
    db_post_events(pchan,  pchan->val,  DBE_VALUE|DBE_LOG);
    db_post_events(pchan,  pchan->mode, DBE_VALUE|DBE_LOG);
    db_post_events(pchan,  pchan->tsmp, DBE_VALUE|DBE_LOG);

    for (int n = 0; n < pchan->nelm; n++) {
        //printf("&pData[%02d]= 0x%08x\n", n, &pData[n]);
        db_post_events(pchan, &pData[ n ], DBE_VALUE|DBE_LOG);
        db_post_events(pchan, &pAlrm[ n ], DBE_VALUE|DBE_LOG|DBE_ALARM);
        db_post_events(pchan, &pUnit[ n ], DBE_VALUE|DBE_LOG);
        db_post_events(pchan, &pPos [ n ], DBE_VALUE|DBE_LOG);
    }

    return;
}

static long cvt_dbaddr(dbAddr *paddr)
{
    channelsRecord *pchan = (channelsRecord *)paddr->precord;

    if (paddr->pfield == &pchan->val) {
        paddr->pfield         = &pchan->ch01;
        paddr->no_elements    = (long)pchan->nelm;
        paddr->field_type     = DBF_DOUBLE;
        paddr->field_size     = sizeof(double);
        paddr->dbr_field_type = DBR_DOUBLE;
        return 0;
    }

    return -1;
}
