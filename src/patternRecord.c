/* patternRecord.c */
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
#include "menuYesNo.h"

#define GEN_SIZE_OFFSET
#include "patternRecord.h"
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
static long get_units();
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
#define get_alarm_double NULL
rset patternRSET={
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
epicsExportAddress(rset,patternRSET);
struct ptndset { /* pattern dset */
        long            number;
        DEVSUPFUN       dev_report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record; /*returns: (-1,0)=>(failure,success)*/
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       read_ptn; /*returns: (-1,0)=>(failure,success)*/
};
/*sizes of field types*/
static int sizeofTypes[] = {0,1,1,2,2,4,4,4,8,2};
static void monitor();
static long readValue();

static long init_record(pptn,pass)
    struct patternRecord	*pptn;
    int pass;
{
    struct ptndset *pdset;
    long status;

    if (pass==0){
	if(pptn->nelm<=0) pptn->nelm=1;
	if(pptn->ftvl >= DBF_CHAR && pptn->ftvl <= DBF_ULONG) {
		pptn->rptr = (char *)calloc(pptn->nelm,sizeofTypes[pptn->ftvl]);
	} else {
                recGblRecordError(S_db_badField,(void *)pptn,"ptn: init_record");
		return(S_db_badField);
	}
	pptn->bptr = (char *)calloc(pptn->nelm,sizeofTypes[DBF_DOUBLE]);
	if(pptn->nelm==1) {
	    pptn->nord = 1;
	} else {
	    pptn->nord = 0;
	}
	return(0);
    }

    /* ptn.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pptn->siml.type == CONSTANT) {
	recGblInitConstantLink(&pptn->siml,DBF_USHORT,&pptn->simm);
    }

    /* must have dset defined */
    if(!(pdset = (struct ptndset *)(pptn->dset))) {
        recGblRecordError(S_dev_noDSET,(void *)pptn,"ptn: init_record");
        return(S_dev_noDSET);
    }
    /* must have read_ptn function defined */
    if( (pdset->number < 5) || (pdset->read_ptn == NULL) ) {
        recGblRecordError(S_dev_missingSup,(void *)pptn,"ptn: init_record");
        return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
        if((status=(*pdset->init_record)(pptn))) return(status);
    }
    return(0);
}

static long process(struct patternRecord *pptn)
{
        struct ptndset   *pdset = (struct ptndset *)(pptn->dset);
	unsigned char    pact=pptn->pact;
	char             *pchar   = (char *)  pptn->rptr;
	unsigned char    *puchar  = (unsigned char *)  pptn->rptr;
	short            *pshort  = (short *) pptn->rptr;
	unsigned short   *pushort = (unsigned short *) pptn->rptr;
	long             *plong   = (long *)  pptn->rptr;
	unsigned long    *pulong  = (unsigned long *)  pptn->rptr;
	double           *bptr = pptn->bptr;
	long		 status;
	int              i;
	   
        if( (pdset==NULL) || (pdset->read_ptn==NULL) ) {
                pptn->pact=TRUE;
                recGblRecordError(S_dev_missingSup,(void *)pptn,"read_ptn");
                return(S_dev_missingSup);
        }

	if ( pact && pptn->busy ) return(0);

	status=readValue(pptn); /* read the new value */

	for (i=0; i < pptn->nelm; i++) {
	    switch (pptn->ftvl) {
	    case DBF_CHAR:
	        bptr[i] = (double) ((pptn->eslo) * (pchar[i]));
		break;
	    case DBF_UCHAR:
	        bptr[i] = (double) ((pptn->eslo) * (puchar[i]));
	        break;
	    case DBF_SHORT:
	        bptr[i] = (double) ((pptn->eslo) * (pshort[i]));
	        break;
	    case DBF_USHORT:
	        bptr[i] = (double) ((pptn->eslo) * (pushort[i]));
	        break;
	    case DBF_LONG:
	        bptr[i] = (double) ((pptn->eslo) * (plong[i]));
	        break;
	    case DBF_ULONG:
	        bptr[i] = (double) ((pptn->eslo) * (pulong[i]));
	        break;
	    defalut:
	        /* never reach here */
	        break;
	    }
	}

	/* check if device support set pact */
	if ( !pact && pptn->pact ) return(0);
        pptn->pact = TRUE;

	pptn->udf=FALSE;
	recGblGetTimeStamp(pptn);

	monitor(pptn);
        /* process the forward scan link record */
        recGblFwdLink(pptn);

        pptn->pact=FALSE;
        return(0);
}

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    struct patternRecord *pptn=(struct patternRecord *)paddr->precord;

    if (paddr->pfield == &pptn->val) {
        paddr->pfield = (void *)(pptn->bptr);
	paddr->no_elements = pptn->nelm;
	paddr->field_type = DBF_DOUBLE;
	paddr->field_size = sizeofTypes[DBF_DOUBLE];
	paddr->dbr_field_type = DBR_DOUBLE;
    }

    if (paddr->pfield == &pptn->rval) {
        paddr->pfield = (void *)(pptn->rptr);
	paddr->no_elements = pptn->nelm;
	paddr->field_type = pptn->ftvl;
	paddr->field_size = sizeofTypes[pptn->ftvl];
	paddr->dbr_field_type = pptn->ftvl;
    }

    return(0);
}


static long get_array_info(paddr,no_elements,offset)
    struct dbAddr *paddr;
    long	  *no_elements;
    long	  *offset;
{
    struct patternRecord	*pptn=(struct patternRecord *)paddr->precord;

    *no_elements =  pptn->nord;
    *offset = 0;
    return(0);
}

static long put_array_info(paddr,nNew)
    struct dbAddr *paddr;
    long	  nNew;
{
    struct patternRecord	*pptn=(struct patternRecord *)paddr->precord;

    pptn->nord = nNew;
    if(pptn->nord > pptn->nelm) pptn->nord = pptn->nelm;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct patternRecord	*pptn=(struct patternRecord *)paddr->precord;

    strncpy(units,pptn->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct patternRecord	*pptn=(struct patternRecord *)paddr->precord;

    *precision = pptn->prec;
    if(paddr->pfield==(void *)pptn->bptr) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble *pgd;
{
    struct patternRecord     *pptn=(struct patternRecord *)paddr->precord;

    if(paddr->pfield==(void *)pptn->bptr){
        pgd->upper_disp_limit = pptn->hopr;
        pgd->lower_disp_limit = pptn->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}
static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct patternRecord     *pptn=(struct patternRecord *)paddr->precord;

    if(paddr->pfield==(void *)pptn->bptr){
        pcd->upper_ctrl_limit = pptn->hopr;
        pcd->lower_ctrl_limit = pptn->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}

static void monitor(pptn)
    struct patternRecord	*pptn;
{
	unsigned short	monitor_mask;

        monitor_mask = recGblResetAlarms(pptn);
	monitor_mask |= (DBE_LOG|DBE_VALUE);
        if(monitor_mask) db_post_events(pptn,pptn->bptr,monitor_mask);
	return;

}

static long readValue(pptn)
        struct patternRecord *pptn;
{
        long            status;
	long		nRequest;
        struct ptndset   *pdset = (struct ptndset *) (pptn->dset);


        if (pptn->pact == TRUE){
                status=(*pdset->read_ptn)(pptn);
                return(status);
        }

        status=dbGetLink(&(pptn->siml), DBR_ENUM,&(pptn->simm),0,0);
        if (status)
                return(status);

        if (pptn->simm == menuYesNoNO){
                status=(*pdset->read_ptn)(pptn);
                return(status);
        }
        if (pptn->simm == menuYesNoYES){
        	nRequest=pptn->nelm;
                status=dbGetLink(&(pptn->siol),
                                pptn->ftvl,pptn->rptr,0,&nRequest);
                /* nord set only for db links: needed for old db_access */
        	if (pptn->siol.type != CONSTANT ){
                pptn->nord = nRequest;
                if (status==0) pptn->udf=FALSE;
            }
        } else {
                status=-1;
                recGblSetSevr(pptn,SOFT_ALARM,INVALID_ALARM);
                return(status);
        }
        recGblSetSevr(pptn,SIMM_ALARM,pptn->sims);

        return(status);
}

