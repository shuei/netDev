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

#ifndef EPICS_REVISION
#include <epicsVersion.h>
#endif

#if EPICS_REVISION < 14
#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<stdlib.h>
#include	<string.h>

#include        "dbDefs.h"
#include        "epicsPrint.h"
#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbEvent.h>
#include	<dbFldTypes.h>
#include	<dbScan.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#else

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
#include "menuOmsl.h"
#include "menuYesNo.h"

#include <stdlib.h>
#include <string.h>
#endif

#define GEN_SIZE_OFFSET
#include "arrayoutRecord.h"
#undef  GEN_SIZE_OFFSET

#if EPICS_REVISION == 14 && EPICS_MODIFICATION >= 2
#include <epicsExport.h>
#endif

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
static long get_units();
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
#define get_alarm_double NULL

#if EPICS_REVISION == 14 && EPICS_MODIFICATION >= 2
rset arrayoutRSET={
#else
struct rset arrayoutRSET={
#endif
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
	get_alarm_double };

#if EPICS_REVISION == 14 && EPICS_MODIFICATION >= 2
epicsExportAddress(rset, arrayoutRSET);
#endif

struct arodset { /* arrayout dset */
        long            number;
        DEVSUPFUN       dev_report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record; /*returns: (-1,0)=>(failure,success)*/
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       write_aro; /*returns: (-1,0)=>(failure,success)*/
};
/*sizes of field types*/
static int sizeofTypes[] = {0,1,1,2,2,4,4,4,8,2};
static void monitor();
static long writeValue();

static long init_record(pao,pass)
    struct arrayoutRecord	*pao;
    int pass;
{
    struct arodset *pdset;
    long status;

    if (pass==0){
	if(pao->nelm<=0) pao->nelm=1;
	if(pao->ftvl == 0) {
		pao->bptr = (char *)calloc(pao->nelm,MAX_STRING_SIZE);
	} else {
		if(pao->ftvl>DBF_ENUM) pao->ftvl=2;
		pao->bptr = (char *)calloc(pao->nelm,sizeofTypes[pao->ftvl]);
	}
	if(pao->nelm==1) {
	    pao->nowt = 1;
	} else {
	    pao->nowt = 0;
	}
	return(0);
    }

    /* aro.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pao->siml.type == CONSTANT) {
	recGblInitConstantLink(&pao->siml,DBF_USHORT,&pao->simm);
    }

    /* must have dset defined */
    if(!(pdset = (struct arodset *)(pao->dset))) {
        recGblRecordError(S_dev_noDSET,(void *)pao,"aro: init_record");
        return(S_dev_noDSET);
    }
    /* must have write_aro function defined */
    if( (pdset->number < 5) || (pdset->write_aro == NULL) ) {
        recGblRecordError(S_dev_missingSup,(void *)pao,"aro: init_record");
        return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
        if((status=(*pdset->init_record)(pao))) return(status);
    }
    return(0);
}

static long process(pao)
	struct arrayoutRecord	*pao;
{
        struct arodset   *pdset = (struct arodset *)(pao->dset);
	long		 status;
	unsigned char    pact = pao->pact;
	long		 nRequest = pao->nelm;

        if( (pdset==NULL) || (pdset->write_aro==NULL) ) {
                pao->pact=TRUE;
                recGblRecordError(S_dev_missingSup,(void *)pao,"write_aro");
                return(S_dev_missingSup);
        }
	if (!pao->pact) {
		if((pao->dol.type != CONSTANT)
                && (pao->omsl == menuOmslclosed_loop)) {
			status = dbGetLink(&(pao->dol),pao->ftvl,pao->bptr,0,&nRequest);
			if (pao->dol.type!=CONSTANT && RTN_SUCCESS(status))
				pao->udf=FALSE;
		}
	}

	if ( pact && pao->busy ) return(0);

	status=writeValue(pao); /* write the new value */
	/* check if device support set pact */
	if ( !pact && pao->pact ) return(0);
        pao->pact = TRUE;

	pao->udf=FALSE;
	recGblGetTimeStamp(pao);

	monitor(pao);
        /* process the forward scan link record */
        recGblFwdLink(pao);

        pao->pact=FALSE;
        return(0);
}

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    struct arrayoutRecord *pao=(struct arrayoutRecord *)paddr->precord;

    paddr->pfield = (void *)(pao->bptr);
    paddr->no_elements = pao->nelm;
    paddr->field_type = pao->ftvl;
    if(pao->ftvl==0)  paddr->field_size = MAX_STRING_SIZE;
    else paddr->field_size = sizeofTypes[pao->ftvl];
    paddr->dbr_field_type = pao->ftvl;
    return(0);
}

static long get_array_info(paddr,no_elements,offset)
    struct dbAddr *paddr;
    long	  *no_elements;
    long	  *offset;
{
    struct arrayoutRecord	*pao=(struct arrayoutRecord *)paddr->precord;

    *no_elements =  pao->nelm;
    *offset = 0;
    return(0);
}

static long put_array_info(paddr,nNew)
    struct dbAddr *paddr;
    long	  nNew;
{
    struct arrayoutRecord	*pao=(struct arrayoutRecord *)paddr->precord;

    pao->nowt = nNew;
    if(pao->nowt > pao->nelm) pao->nowt = pao->nelm;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct arrayoutRecord	*pao=(struct arrayoutRecord *)paddr->precord;

    strncpy(units,pao->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct arrayoutRecord	*pao=(struct arrayoutRecord *)paddr->precord;

    *precision = pao->prec;
    if(paddr->pfield==(void *)pao->bptr) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble *pgd;
{
    struct arrayoutRecord     *pao=(struct arrayoutRecord *)paddr->precord;

    if(paddr->pfield==(void *)pao->bptr){
        pgd->upper_disp_limit = pao->hopr;
        pgd->lower_disp_limit = pao->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}
static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct arrayoutRecord     *pao=(struct arrayoutRecord *)paddr->precord;

    if(paddr->pfield==(void *)pao->bptr){
        pcd->upper_ctrl_limit = pao->hopr;
        pcd->lower_ctrl_limit = pao->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}

static void monitor(pao)
    struct arrayoutRecord	*pao;
{
	unsigned short	monitor_mask;

        monitor_mask = recGblResetAlarms(pao);
	monitor_mask |= (DBE_LOG|DBE_VALUE);
        if(monitor_mask) db_post_events(pao,pao->bptr,monitor_mask);
	return;

}

static long writeValue(pao)
        struct arrayoutRecord *pao;
{
        long            status;
	long		nRequest;
        struct arodset   *pdset = (struct arodset *) (pao->dset);


        if (pao->pact == TRUE){
                status=(*pdset->write_aro)(pao);
                return(status);
        }

        status=dbGetLink(&(pao->siml), DBR_ENUM,&(pao->simm),0,0);
        if (status)
                return(status);

        if (pao->simm == menuYesNoNO){
                status=(*pdset->write_aro)(pao);
                return(status);
        }
        if (pao->simm == menuYesNoYES){
        	nRequest=pao->nelm;
                status=dbPutLink(&(pao->siol),
				 pao->ftvl,pao->bptr,nRequest);
                /* nowt set only for db links: needed for old db_access */
        	if (pao->siol.type != CONSTANT ){
                pao->nowt = nRequest;
                if (status==0) pao->udf=FALSE;
            }
        } else {
                status=-1;
                recGblSetSevr(pao,SOFT_ALARM,INVALID_ALARM);
                return(status);
        }
        recGblSetSevr(pao,SIMM_ALARM,pao->sims);

        return(status);
}
