/* devMiwfSoft.c */
/****************************************************************************
 *                         COPYRIGHT NOTIFICATION
 *
 * Copyright (c) All rights reserved
 *
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 ****************************************************************************/
/* Current Author: Jun-ichi Odagiri (jun-ichi.odagiri@kek.jp, KEK) */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <alarm.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <epicsExport.h>
#include <recGbl.h>
#include <recSup.h>
#include <devSup.h>
#include <link.h>

#include "miwfRecord.h"

// Create the dset for devMiwfSoft
static long init_record();
static long read_wf();
struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_wf;
} devMiwfSoft = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_wf
};
epicsExportAddress(dset,devMiwfSoft);

#define check_link(inp, msg)  \
    switch ((inp).type) {     \
    case (CONSTANT):          \
        break;                \
    case (PV_LINK) :          \
    case (DB_LINK) :          \
    case (CA_LINK) :          \
        linkflag=1;           \
        break;                \
    default :                 \
        recGblRecordError(S_db_badField, pmiwf, msg);\
        return S_db_badField; \
    }

static long init_record(miwfRecord *pmiwf)
{
    int linkflag = 0;
    // wf.inp must be a PV_LINK, a DB_LINK, or a CA_LINK
    check_link(pmiwf->inp0, "devMiwfSoft (init_record) Illegal INP0 field");
    check_link(pmiwf->inp1, "devMiwfSoft (init_record) Illegal INP1 field");
    check_link(pmiwf->inp2, "devMiwfSoft (init_record) Illegal INP2 field");
    check_link(pmiwf->inp3, "devMiwfSoft (init_record) Illegal INP3 field");
    check_link(pmiwf->inp4, "devMiwfSoft (init_record) Illegal INP4 field");
    check_link(pmiwf->inp5, "devMiwfSoft (init_record) Illegal INP5 field");
    check_link(pmiwf->inp6, "devMiwfSoft (init_record) Illegal INP6 field");

    if (linkflag == 0) {
        recGblRecordError(S_db_badField, pmiwf, "devMiwfSoft (init_record) No input link specified.");
        return S_db_badField;
    }
    return 0;
}
#undef chekc_link

static long read_wf(miwfRecord *pmiwf)
{
    DBLINK *plink;
    switch (pmiwf->seln) {
    case 0:
        plink = &pmiwf->inp0;
        break;
    case 1:
        plink = &pmiwf->inp1;
        break;
    case 2:
        plink = &pmiwf->inp2;
        break;
    case 3:
        plink = &pmiwf->inp3;
        break;
    case 4:
        plink = &pmiwf->inp4;
        break;
    case 5:
        plink = &pmiwf->inp5;
        break;
    case 6:
        plink = &pmiwf->inp6;
        break;
    default:
        recGblRecordError(S_db_badField, pmiwf, "devMiwfSoft (read_wf) Illegal SELN field");
        return S_db_badField;
    }

    long nRequest = pmiwf->nelm;

    // long status =
    dbGetLink(plink, pmiwf->ftvl, pmiwf->bptr, 0, &nRequest);

    // If dbGetLink got no values leave things as they were
    if (nRequest > 0) {
        pmiwf->nord = nRequest;
    }

    return 0;
}
