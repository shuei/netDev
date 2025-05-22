/* devArrayoutYewPlc.c */
/****************************************************************************
 *                         COPYRIGHT NOTIFICATION
 *
 * Copyright (c) All rights reserved
 *
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 ****************************************************************************/
/* Author: Jun-ichi Odagiri (jun-ichi.odagiri@kek.jp, KEK) */
/* Modification Log:
 * -----------------
 * 2005/08/22 jio
 *  passing option flag to Link Field Parser was changed from by value to
 * by pointer
 */

#include <arrayoutRecord.h>

//
// Arrayout (command/response IO)
//
static long init_arrayout_record(arrayoutRecord *);
static long write_arrayout(arrayoutRecord *);
static long config_arrayout_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_arrayout_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devAroYewPlc = {
    5,
    NULL,
    netDevInit,
    init_arrayout_record,
    NULL,
    write_arrayout
};

epicsExportAddress(dset, devAroYewPlc);

static long init_arrayout_record(arrayoutRecord *prec)
{
    //DEBUG
    //if (netDevDebug>0) {
    //    printf("%s: %s %s\n", __FILE__, __func__, prec->name);
    //}

    YEW_PLC *d = yew_calloc(0, 0, 0, kWord);
    long ret = netDevInitXxRecord((dbCommon *)prec,
                                  &prec->out,
                                  MPF_WRITE | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                                  d,
                                  yew_parse_link,
                                  config_arrayout_command,
                                  parse_arrayout_response
                                  );

    char *ftvlstr = pamapdbfType[prec->ftvl].strvalue;
    ftvlstr += 4;

    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %s %s, ftvl=%s(%d) flag=%c\n", __FILE__, __func__, prec->name, ftvlstr, prec->ftvl, d->flag);
    }

#if 0
    if (0) {
        //
    } else if (prec->ftvl==DBF_DOUBLE) {
        if (d->flag!='D') {
            errlogPrintf("%s: &D option is required when FTVL is %s\n", prec->name, ftvlstr);
            recGblSetSevr(prec, INVALID_ALARM, INVALID_ALARM);
            return ERROR;
        }
    } else if (prec->ftvl==DBF_FLOAT) {
        if (d->flag!='F') {
            errlogPrintf("%s: &F option is required when FTVL is %s\n", prec->name, ftvlstr);
            recGblSetSevr(prec, INVALID_ALARM, INVALID_ALARM);
            return ERROR;
        }
    } else if (prec->ftvl==DBF_SHORT || prec->ftvl==DBF_USHORT || prec->ftvl==DBF_LONG || prec->ftvl==DBF_ULONG) {
        if (d->flag=='F' || d->flag=='D') {
            errlogPrintf("%s: unsupported conbination of FTVL (%s) and option(&%c)\n", prec->name, ftvlstr, d->flag);
            recGblSetSevr(prec, INVALID_ALARM, INVALID_ALARM);
            return ERROR;
        }
    } else {
        errlogPrintf("%s: unsupported of FTVL (%s)\n", prec->name, ftvlstr);
        recGblSetSevr(prec, INVALID_ALARM, INVALID_ALARM);
        return ERROR;
    }
#endif

    return ret;
}

static long write_arrayout(arrayoutRecord *prec)
{
    TRANSACTION *t = prec->dpvt;
    YEW_PLC *d = t->device;

    // make sure that those below are cleared in the event that
    // a multi-step transfer is terminated by an error in the
    // middle of transacton
    d->nleft = 0;
    d->noff = 0;

    return netDevReadWriteXx((dbCommon *)prec);
}

static long config_arrayout_command(dbCommon *pxx,
                                    int *option,
                                    uint8_t *buf,
                                    int *len,
                                    void *device,
                                    int transaction_id
                                    )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("\n%s: %s %s\n", __FILE__, __func__, pxx->name);
    }

    arrayoutRecord *prec = (arrayoutRecord *)pxx;

    return yew_config_command(buf,
                              len,
                              prec->bptr,
                              prec->ftvl, // This has nothing to do with options such as \&F or \&L
                              prec->nelm,
                              option,
                              device
                              );
}

static long parse_arrayout_response(dbCommon *pxx,
                                    int *option,
                                    uint8_t *buf,
                                    int *len,
                                    void *device,
                                    int transaction_id
                                    )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("\n%s: %s %s\n", __FILE__, __func__, pxx->name);
    }

    arrayoutRecord *prec = (arrayoutRecord *)pxx;
    YEW_PLC *d = device;

    long ret = yew_parse_response(buf,
                                  len,
                                  0, // not used in yew_parse_response
                                  0, // not used in yew_parse_response
                                  prec->nelm,
                                  option,
                                  d
                                  );

    switch (ret) {
    case NOT_DONE:
        prec->nowt = d->noff;
        // why we don't have break here?
    case 0:
        prec->nowt = prec->nelm;
    default:
        ;
    }

    return ret;
}
