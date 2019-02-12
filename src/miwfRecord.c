/* miwfRecord.c */
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
#include "miwfRecord.h"
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
rset miwfRSET={
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

epicsExportAddress(rset,miwfRSET);

struct wfdset { /* miwf dset */
        long            number;
        DEVSUPFUN       dev_report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record; /*returns: (-1,0)=>(failure,success)*/
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       read_wf; /*returns: (-1,0)=>(failure,success)*/
};
/*sizes of field types*/
static int sizeofTypes[] = {0,1,1,2,2,4,4,4,8,2};
static void monitor();
static long readValue();

static long init_record(mipwf,pass)
    struct miwfRecord	*mipwf;
    int pass;
{
    struct wfdset *pdset;
    long status;

    if (pass==0){
	if(mipwf->nelm<=0) mipwf->nelm=1;
	if(mipwf->ftvl == 0) {
		mipwf->bptr = (char *)calloc(mipwf->nelm,MAX_STRING_SIZE);
	} else {
		if(mipwf->ftvl>DBF_ENUM) mipwf->ftvl=2;
		mipwf->bptr = (char *)calloc(mipwf->nelm,sizeofTypes[mipwf->ftvl]);
	}
	if(mipwf->nelm==1) {
	    mipwf->nord = 1;
	} else {
	    mipwf->nord = 0;
	}
	recGblInitConstantLink(&mipwf->sell,DBF_USHORT,&mipwf->seln);
	return(0);
    }

    /* wf.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (mipwf->siml.type == CONSTANT) {
	recGblInitConstantLink(&mipwf->siml,DBF_USHORT,&mipwf->simm);
    }

    /* must have dset defined */
    if(!(pdset = (struct wfdset *)(mipwf->dset))) {
        recGblRecordError(S_dev_noDSET,(void *)mipwf,"wf: init_record");
        return(S_dev_noDSET);
    }
    /* must have read_wf function defined */
    if( (pdset->number < 5) || (pdset->read_wf == NULL) ) {
        recGblRecordError(S_dev_missingSup,(void *)mipwf,"wf: init_record");
        return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
        if((status=(*pdset->init_record)(mipwf))) return(status);
    }
    return(0);
}

static long process(mipwf)
	struct miwfRecord	*mipwf;
{
        struct wfdset   *pdset = (struct wfdset *)(mipwf->dset);
	long		 status;
	unsigned char    pact=mipwf->pact;

        if( (pdset==NULL) || (pdset->read_wf==NULL) ) {
                mipwf->pact=TRUE;
                recGblRecordError(S_dev_missingSup,(void *)mipwf,"read_wf");
                return(S_dev_missingSup);
        }

	if ( pact && mipwf->busy ) return(0);

	status=readValue(mipwf); /* read the new value */
	/* check if device support set pact */
	if ( !pact && mipwf->pact ) return(0);
        mipwf->pact = TRUE;

	mipwf->udf=FALSE;
	recGblGetTimeStamp(mipwf);

	monitor(mipwf);
        /* process the forward scan link record */
        recGblFwdLink(mipwf);

        mipwf->pact=FALSE;
        return(0);
}

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    struct miwfRecord *mipwf=(struct miwfRecord *)paddr->precord;

    paddr->pfield = (void *)(mipwf->bptr);
    paddr->no_elements = mipwf->nelm;
    paddr->field_type = mipwf->ftvl;
    if(mipwf->ftvl==0)  paddr->field_size = MAX_STRING_SIZE;
    else paddr->field_size = sizeofTypes[mipwf->ftvl];
    paddr->dbr_field_type = mipwf->ftvl;
    return(0);
}

static long get_array_info(paddr,no_elements,offset)
    struct dbAddr *paddr;
    long	  *no_elements;
    long	  *offset;
{
    struct miwfRecord	*mipwf=(struct miwfRecord *)paddr->precord;

    *no_elements =  mipwf->nord;
    *offset = 0;
    return(0);
}

static long put_array_info(paddr,nNew)
    struct dbAddr *paddr;
    long	  nNew;
{
    struct miwfRecord	*mipwf=(struct miwfRecord *)paddr->precord;

    mipwf->nord = nNew;
    if(mipwf->nord > mipwf->nelm) mipwf->nord = mipwf->nelm;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct miwfRecord	*mipwf=(struct miwfRecord *)paddr->precord;

    strncpy(units,mipwf->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct miwfRecord	*mipwf=(struct miwfRecord *)paddr->precord;

    *precision = mipwf->prec;
    if(paddr->pfield==(void *)mipwf->bptr) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble *pgd;
{
    struct miwfRecord     *mipwf=(struct miwfRecord *)paddr->precord;

    if(paddr->pfield==(void *)mipwf->bptr){
        pgd->upper_disp_limit = mipwf->hopr;
        pgd->lower_disp_limit = mipwf->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}
static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct miwfRecord     *mipwf=(struct miwfRecord *)paddr->precord;

    if(paddr->pfield==(void *)mipwf->bptr){
        pcd->upper_ctrl_limit = mipwf->hopr;
        pcd->lower_ctrl_limit = mipwf->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}

static void monitor(mipwf)
    struct miwfRecord	*mipwf;
{
	unsigned short	monitor_mask;

        monitor_mask = recGblResetAlarms(mipwf);
	monitor_mask |= (DBE_LOG|DBE_VALUE);
        if(monitor_mask) db_post_events(mipwf,mipwf->bptr,monitor_mask);
	return;

}

static long readValue(mipwf)
        struct miwfRecord *mipwf;
{
        long            status;
	long		nRequest;
        struct wfdset   *pdset = (struct wfdset *) (mipwf->dset);


        if (mipwf->pact == TRUE){
                status=(*pdset->read_wf)(mipwf);
                return(status);
        }

        status=dbGetLink(&(mipwf->siml), DBR_ENUM,&(mipwf->simm),0,0);
        if (status)
                return(status);

        if (mipwf->simm == menuYesNoNO){
		status=dbGetLink(&(mipwf->sell),DBR_USHORT,&(mipwf->seln),0,0);
		if (status)
		        return(status);
                status=(*pdset->read_wf)(mipwf);
                return(status);
        }
        if (mipwf->simm == menuYesNoYES){
        	nRequest=mipwf->nelm;
                status=dbGetLink(&(mipwf->siol),
                                mipwf->ftvl,mipwf->bptr,0,&nRequest);
                /* nord set only for db links: needed for old db_access */
        	if (mipwf->siol.type != CONSTANT ){
                mipwf->nord = nRequest;
                if (status==0) mipwf->udf=FALSE;
            }
        } else {
                status=-1;
                recGblSetSevr(mipwf,SOFT_ALARM,INVALID_ALARM);
                return(status);
        }
        recGblSetSevr(mipwf,SIMM_ALARM,mipwf->sims);

        return(status);
}

