/* arrayoutRecord.c */
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

#ifdef vxWorks
#  include <vxWorks.h>
#  include <types.h>
#  include <stdioLib.h>
#  include <lstLib.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <alarm.h>
#include <dbAccess.h>
#include <dbDefs.h>
#include <dbEvent.h>
#include <dbFldTypes.h>
#include <dbScan.h>
#include <devSup.h>
#include <epicsExport.h>
#include <epicsPrint.h>
#include <errMdef.h>
#include <recSup.h>
#include <recGbl.h>
#include <menuOmsl.h>
#include <menuYesNo.h>

#define GEN_SIZE_OFFSET
#include "arrayoutRecord.h"
#undef  GEN_SIZE_OFFSET

// Create RSET - Record Support Entry Table
#define report NULL
#define initialize NULL
static long init_record();
static long process();
#define special NULL
#define get_value NULL
static long cvt_dbaddr();
static long get_array_info();
static long put_array_info();
static long get_units();
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
#define get_alarm_double NULL

rset arrayoutRSET = {
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

epicsExportAddress(rset, arrayoutRSET);

// arrayout dset
typedef struct arodset {
        long            number;
        DEVSUPFUN       dev_report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record; // returns: (-1,0)=>(failure,success)
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       write_aro;   // returns: (-1,0)=>(failure,success)
} arodset;

static void monitor();
static long writeValue();

static long init_record(arrayoutRecord *pao, int pass)
{
    if (pass == 0) {
        if (pao->nelm <= 0) {
            pao->nelm = 1;
        }
        if (pao->ftvl == 0) {
            pao->bptr = calloc(pao->nelm, MAX_STRING_SIZE);
        } else {
            if (pao->ftvl > DBF_ENUM) {
                pao->ftvl = 2;
            }
            pao->bptr = calloc(pao->nelm, dbValueSize(pao->ftvl));
        }
        if (pao->nelm == 1) {
            pao->nowt = 1;
        } else {
            pao->nowt = 0;
        }
        return 0;
    }

    // aro.siml must be a CONSTANT, a PV_LINK, or a DB_LINK
    if (pao->siml.type == CONSTANT) {
        recGblInitConstantLink(&pao->siml, DBF_USHORT, &pao->simm);
    }

    // must have dset defined
    arodset *pdset = (arodset *)(pao->dset);
    if (!pdset) {
        recGblRecordError(S_dev_noDSET, pao, "arrayout: No DSET");
        return S_dev_noDSET;
    }

    // must have write_aro function defined
    if ((pdset->number < 5) || (pdset->write_aro == NULL)) {
        recGblRecordError(S_dev_missingSup, pao, "arrayout: Bad DSET");
        return S_dev_missingSup;
    }

    if (pdset->init_record) {
        long status = (*pdset->init_record)(pao);
        if (status) {
            return status;
        }
    }
    return 0;
}

static long process(arrayoutRecord *pao)
{
    arodset *pdset = (arodset *)(pao->dset);
    unsigned char   pact = pao->pact;
    long            nRequest = pao->nelm;

    if ((pdset == NULL) || (pdset->write_aro == NULL)) {
        pao->pact = TRUE;
        recGblRecordError(S_dev_missingSup, pao, "write_arrayout");
        return S_dev_missingSup;
    }

    if (!pao->pact) {
        if ((pao->dol.type != CONSTANT)
           && (pao->omsl == menuOmslclosed_loop)) {
            long status = dbGetLink(&(pao->dol), pao->ftvl, pao->bptr, 0, &nRequest);
            if (pao->dol.type != CONSTANT && RTN_SUCCESS(status)) {
                pao->udf = FALSE;
            }
        }
    }

    if (pact && pao->busy) {
        return 0;
    }

    //long status =
    writeValue(pao); // write the new value

    // check if device support set pact
    if (!pact && pao->pact) {
        return 0;
    }

    pao->pact = TRUE;
    pao->udf = FALSE;
    recGblGetTimeStamp(pao);

    monitor(pao);

    // process the forward scan link record
    recGblFwdLink(pao);

    pao->pact = FALSE;
    return 0;
}

static long cvt_dbaddr(dbAddr *paddr)
{
    arrayoutRecord *pao = (arrayoutRecord *)paddr->precord;

    paddr->pfield      = pao->bptr;
    paddr->no_elements = pao->nelm;
    paddr->field_type  = pao->ftvl;
    if (pao->ftvl == 0) {
        paddr->field_size = MAX_STRING_SIZE;
    } else {
        paddr->field_size = dbValueSize(pao->ftvl);
    }
    paddr->dbr_field_type = pao->ftvl;
    return 0;
}

static long get_array_info(dbAddr *paddr, long *no_elements, long *offset)
{
    arrayoutRecord *pao = (arrayoutRecord *)paddr->precord;

    *no_elements = pao->nelm;
    *offset = 0;
    return 0;
}

static long put_array_info(dbAddr *paddr, long nNew)
{
    arrayoutRecord *pao = (arrayoutRecord *)paddr->precord;

    pao->nowt = nNew;
    if (pao->nowt > pao->nelm) {
        pao->nowt = pao->nelm;
    }
    return 0;
}

static long get_units(dbAddr *paddr, char *units)
{
    arrayoutRecord *pao = (arrayoutRecord *)paddr->precord;

    strncpy(units, pao->egu, DB_UNITS_SIZE);
    return 0;
}

static long get_precision(dbAddr *paddr, long *precision)
{
    arrayoutRecord *pao = (arrayoutRecord *)paddr->precord;

    *precision = pao->prec;
    if (paddr->pfield == pao->bptr) {
        return 0;
    }
    recGblGetPrec(paddr, precision);
    return 0;
}

static long get_graphic_double(dbAddr *paddr, struct dbr_grDouble *pgd)
{
    arrayoutRecord *pao = (arrayoutRecord *)paddr->precord;

    if (paddr->pfield == pao->bptr) {
        pgd->upper_disp_limit = pao->hopr;
        pgd->lower_disp_limit = pao->lopr;
    } else {
        recGblGetGraphicDouble(paddr, pgd);
    }
    return 0;
}

static long get_control_double(dbAddr *paddr, struct dbr_ctrlDouble *pcd)
{
    arrayoutRecord *pao = (arrayoutRecord *)paddr->precord;

    if (paddr->pfield == pao->bptr) {
        pcd->upper_ctrl_limit = pao->hopr;
        pcd->lower_ctrl_limit = pao->lopr;
    } else {
        recGblGetControlDouble(paddr, pcd);
    }
    return 0;
}

static void monitor(arrayoutRecord *pao)
{
    unsigned short monitor_mask;

    monitor_mask = recGblResetAlarms(pao);
    monitor_mask |= (DBE_LOG|DBE_VALUE);
    if (monitor_mask) {
        db_post_events(pao, pao->bptr, monitor_mask);
    }
    return;
}

static long writeValue(arrayoutRecord *pao)
{
    arodset *pdset = (arodset *)(pao->dset);

    long status;
    if (pao->pact == TRUE) {
        status = (*pdset->write_aro)(pao);
        return status;
    }

    status = dbGetLink(&(pao->siml), DBR_ENUM,&(pao->simm), 0, 0);
    if (status) {
        return status;
    }

    if (pao->simm == menuYesNoNO) {
        status = (*pdset->write_aro)(pao);
        return status;
    }

    if (pao->simm == menuYesNoYES) {
        long nRequest = pao->nelm;
        status=dbPutLink(&(pao->siol), pao->ftvl, pao->bptr, nRequest);

        // nowt set only for db links: needed for old db_access
        if (pao->siol.type != CONSTANT) {
            pao->nowt = nRequest;
            if (status == 0) {
                pao->udf = FALSE;
            }
        }
    } else {
        status = -1;
        recGblSetSevr(pao, SOFT_ALARM, INVALID_ALARM);
        return status;
    }
    recGblSetSevr(pao, SIMM_ALARM, pao->sims);

    return status;
}
