/* statusRecord.c */
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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "alarm.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "dbScan.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#define GEN_SIZE_OFFSET
#include "statusRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
#define special NULL
#define get_value NULL
static long cvt_dbaddr();
static long get_array_info();
static long put_array_info();
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

rset statusRSET = {
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

epicsExportAddress(rset,statusRSET);

struct statusdset { /* status dset */
    long      number;
    DEVSUPFUN dev_report;
    DEVSUPFUN init;
    DEVSUPFUN init_record; /*returns: (-1,0)=>(failure,success)*/
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_status; /*returns: (-1,0)=>(failure,success)*/
};

/*sizes of field types*/
static int sizeofTypes[] = {0,1,1,2,2,4,4,4,8,2};

static void monitor();
static long readValue();

static long init_record(struct statusRecord *pst, int pass)
{
    struct statusdset *pdset;
    long status;

    if (pass==0) {
        if (pst->nelm<=0) {
            pst->nelm=1;
        }

        if (pst->nelm==1) {
            pst->nord = 1;
        } else {
            pst->nord = 0;
        }

        return 0;
    }

    /* must have dset defined */
    if (!(pdset = (struct statusdset *)(pst->dset))) {
        recGblRecordError(S_dev_noDSET,(void *)pst,"status: init_record");
        return S_dev_noDSET;
    }

    /* must have read_status function defined */
    if ((pdset->number < 5) || (pdset->read_status == NULL)) {
        recGblRecordError(S_dev_missingSup,(void *)pst,"status: init_record");
        return S_dev_missingSup;
    }

    if (pdset->init_record) {
        if ((status=(*pdset->init_record)(pst))) {
            return status;
        }
    }

    return 0;
}

static long process(struct statusRecord *pst)
{
    struct statusdset *pdset = (struct statusdset *)(pst->dset);
    unsigned char pact = pst->pact;

    if ((pdset==NULL) || (pdset->read_status==NULL)) {
        pst->pact=TRUE;
        recGblRecordError(S_dev_missingSup,(void *)pst,"read_status");
        return S_dev_missingSup;
    }

    /* status= */
    readValue(pst); /* read the new value */

    /* check if device support set pact */
    if (!pact && pst->pact) {
      return 0;
    }
    pst->pact = TRUE;

    pst->udf=FALSE;
    recGblGetTimeStamp(pst);

    monitor(pst);

    /* process the forward scan link record */
    recGblFwdLink(pst);

    pst->pact=FALSE;
    return 0;
}

static long cvt_dbaddr(struct dbAddr *paddr)
{
    struct statusRecord *pst=(struct statusRecord *)paddr->precord;

    paddr->pfield = (void *) &(pst->ch01);
    paddr->no_elements = pst->nelm;
    paddr->field_type = DBF_USHORT;
    paddr->field_size = sizeofTypes[DBF_USHORT];
    paddr->dbr_field_type = DBR_USHORT;
    return 0;
}

static long get_array_info(struct dbAddr *paddr, long *no_elements, long *offset)
{
    struct statusRecord *pst = (struct statusRecord *)paddr->precord;

    *no_elements =  pst->nord;
    *offset = 0;
    return 0;
}

static long put_array_info(struct dbAddr *paddr, long nNew)
{
    struct statusRecord *pst = (struct statusRecord *)paddr->precord;

    pst->nord = nNew;
    if (pst->nord > pst->nelm) {
        pst->nord = pst->nelm;
    }
    return 0;
}

static void monitor(struct statusRecord *pst)
{
    unsigned short monitor_mask;

    monitor_mask = recGblResetAlarms(pst);
    monitor_mask |= (DBE_LOG|DBE_VALUE);
    if (monitor_mask) {
        db_post_events(pst,&pst->ch01,monitor_mask);
    }
    return;
}

static long readValue(struct statusRecord *pst)
{
    long status = 0; // avoid use of uninitialized value; is this OK?
    struct statusdset *pdset = (struct statusdset *) (pst->dset);

    if (pst->pact == TRUE) {
        status=(*pdset->read_status)(pst);
        return status; /* it seems that return value is not used at all */
    }

    return status; /* it seems that return value is not used at all */
}
