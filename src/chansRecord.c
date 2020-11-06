/* chansRecord.c */
/******************************************************************************
 *                         COPYRIGHT NOTIFICATION
 *
 * Copyright (c) All rights reserved
 *
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 * This work is based on waveformRecord.c
 ******************************************************************************/
/* Current Author: Jun-ichi Odagiri (jun-ichi.odagiri@kek.jp, KEK) */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <alarm.h>
#include <dbAccess.h>
#include <dbEvent.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <devSup.h>
#include <epicsExport.h>
#include <errMdef.h>
#include <recGbl.h>
#include <recSup.h>
#include <special.h>

#define GEN_SIZE_OFFSET
#include "chansRecord.h"
#undef  GEN_SIZE_OFFSET

/* Create RSET - Record Support Entry Table */
#define report NULL
#define initialize NULL
static long init_record();
static long process();
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

struct rset chansRSET = {
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

epicsExportAddress(rset, chansRSET);

typedef struct chans_dset { /* chans dset */
    long      number;
    DEVSUPFUN dev_report;
    DEVSUPFUN init;
    DEVSUPFUN init_record; /*returns: (-1,0)=>(failure,success)*/
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_chans;
} chans_dset;

static void checkAlarms(chansRecord *pchans);
static void monitor(chansRecord *pchans);

static long init_record(void *precord, int pass)
{
    chansRecord *pchans = (chansRecord *)precord;

    if (pass == 0) {
        return 0;
    }

    chans_dset  *pdset;
    if (!(pdset = (chans_dset *)(pchans->dset))) {
        recGblRecordError(S_dev_noDSET, (void *)pchans, "chans: No DSET");
        return S_dev_noDSET;
    }

    /* must have read_chans function defined */
    if ((pdset->number < 5) || (pdset->read_chans == NULL)) {
        recGblRecordError(S_dev_missingSup, (void *)pchans, "chans: Bad DSET");
        return S_dev_missingSup;
    }

    if (pdset->init_record) {
        long status;
        if ((status=(*pdset->init_record)(pchans))) {
            return status;
        }
    }

    return 0;
}

static long process(void *precord)
{
    chansRecord *pchans = (chansRecord *)precord;
    chans_dset  *pdset = (chans_dset *)(pchans->dset);

    if ((pdset==NULL) || (pdset->read_chans==NULL)) {
        pchans->pact=TRUE;
        recGblRecordError(S_dev_missingSup,(void *)pchans,"read_chans");
        return S_dev_missingSup;
    }

    /* pact must not be set until after calling device support */
    unsigned char pact = pchans->pact;
    long status = (*pdset->read_chans)(pchans);

    /* check if device support set pact */
    if (!pact && pchans->pact) {
        return 0;
    }
    pchans->pact = TRUE;

    recGblGetTimeStamp(pchans);
    /* check for alarms */
    checkAlarms(pchans);
    /* check event list */
    monitor(pchans);
    /* process the forward scan link record */
    recGblFwdLink(pchans);

    pchans->pact=FALSE;

    return status;
}

static long get_precision(DBADDR *paddr, long *precision)
{
    chansRecord *pchans=(chansRecord *)paddr->precord;

    *precision = pchans->prec;
    if (paddr->pfield == (void *) &pchans->ch01) {
        return 0;
    }
    recGblGetPrec(paddr,precision);
    return 0;
}

static void checkAlarms(chansRecord *pchans)
{
  return;
}

static void monitor(chansRecord *pchans)
{
    double *data = (double *) &pchans->ch01;
    char  (*stat)[8] = (char (*)[8]) pchans->st01;
    char  (*alrm)[8] = (char (*)[8]) pchans->al01;
    char  (*unit)[8] = (char (*)[8]) pchans->eu01;
    int n = 0;

    recGblResetAlarms(pchans);

    db_post_events(pchans,  pchans->val,  DBE_VALUE|DBE_LOG);
    db_post_events(pchans,  pchans->date, DBE_VALUE|DBE_LOG);
    db_post_events(pchans,  pchans->atim, DBE_VALUE|DBE_LOG);

    for (n = 0; n < pchans->noch; n++) {
        /*
          printf("&data[%02d]= 0x%08x\n", n, &data[n]);
        */
        db_post_events(pchans, &data[n], DBE_VALUE|DBE_LOG);
        db_post_events(pchans,  stat[n], DBE_VALUE|DBE_LOG);
        db_post_events(pchans,  alrm[n], DBE_VALUE|DBE_LOG);
        db_post_events(pchans,  unit[n], DBE_VALUE|DBE_LOG);
    }

    return;
}

static long cvt_dbaddr(struct dbAddr *paddr)
{
    chansRecord *pchans = (chansRecord *) paddr->precord;

    if (paddr->pfield == &pchans->val) {
        paddr->pfield         = (void *) &pchans->ch01;
        paddr->no_elements    = (long) pchans->noch;
        paddr->field_type     = DBF_DOUBLE;
        paddr->field_size     = sizeof(double);
        paddr->dbr_field_type = DBR_DOUBLE;
        return 0;
    }

    return -1;
}
