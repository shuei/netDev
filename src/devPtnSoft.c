/* devPtnSoft.c */
/******************************************************************************
 *                         COPYRIGHT NOTIFICATION
 *
 * Copyright (c) All rights reserved
 *
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 ******************************************************************************/
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

#include "patternRecord.h"

// Create the dset for devPtnSoft
static long init_record();
static long read_wf();

struct {
    long        number;
    DEVSUPFUN    report;
    DEVSUPFUN    init;
    DEVSUPFUN    init_record;
    DEVSUPFUN    get_ioint_info;
    DEVSUPFUN    read_wf;
} devPtnSoft = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_wf
};

epicsExportAddress(dset, devPtnSoft);

static long init_record(patternRecord *pptn)
{
    // wf.inp must be a CONSTANT, a PV_LINK, a DB_LINK, or a CA_LINK
    switch (pptn->inp.type) {
    case (CONSTANT) :
        pptn->nord = 0;
        break;
    case (PV_LINK) :
    case (DB_LINK) :
    case (CA_LINK) :
        break;
    default :
        recGblRecordError(S_db_badField, pptn, "devPtnSoft (init_record) Illegal INP field");
        return S_db_badField;
    }

    return 0;
}

static long read_wf(patternRecord *pptn)
{
    long nRequest = pptn->nelm;
    // long status =
    dbGetLink(&pptn->inp, pptn->ftvl,pptn->bptr, 0, &nRequest);

    // If dbGetLink got no values leave things as they were
    if (nRequest>0) {
        pptn->nord = nRequest;
    }

    return 0;
}
