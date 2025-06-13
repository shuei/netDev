/* devWaveformYewPlc.c */
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

#include <waveformRecord.h>

//
// Waveform (command/response IO)
//
static long init_waveform_record(waveformRecord *);
static long read_waveform(waveformRecord *);
static long config_waveform_command(dbCommon *, int *, uint8_t *, int *, void *, int);
static long parse_waveform_response(dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devWfYewPlc = {
    5,
    NULL,
    netDevInit,
    init_waveform_record,
    NULL,
    read_waveform
};

epicsExportAddress(dset, devWfYewPlc);

static long init_waveform_record(waveformRecord *prec)
{
    //DEBUG
    //if (netDevDebug>0) {
    //    printf("%s: %s %s\n", __FILE__, __func__, prec->name);
    //}

    YEW_PLC *d = yew_calloc(0, 0, 0, kWord);
    long ret = netDevInitXxRecord((dbCommon *)prec,
                                  &prec->inp,
                                  MPF_READ | YEW_GET_PROTO | DEFAULT_TIMEOUT,
                                  d,
                                  yew_parse_link,
                                  config_waveform_command,
                                  parse_waveform_response
                                  );

    d->val = calloc(prec->nelm, convsize[d->conv]);
    if (! d->val) {
        errlogPrintf("%s: %s : calloc failed\n", __func__, prec->name);
        prec->pact = 1;
        return -1;
    }

    //
    const char *ftvlstr = (pamapdbfType[prec->ftvl].strvalue) + 4;

    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %s %s, ftvl=%s(%d) conv=%s\n", __FILE__, __func__, prec->name, ftvlstr, prec->ftvl, convstr[d->conv]);
    }

    // check conversion option
    if (0) {
    } else if (prec->ftvl==DBF_DOUBLE) {
        if (0) {
        } else if (d->conv == kSHORT) {
        } else if (d->conv == kUSHORT) {
        } else if (d->conv == kLONG) {
        //} else if (d->conv == kULONG) {
        } else if (d->conv == kFLOAT) {
        } else if (d->conv == kDOUBLE) {
        //} else if (d->conv == kBCD) {
        } else {
            errlogPrintf("%s: %s : unsupported conversion \"&%s\" with FTVL = %s\n", __func__, ftvlstr, convstr[d->conv], prec->name);
            prec->pact = 1;
            return -1;
        }
    } else if (prec->ftvl==DBF_FLOAT) {
        if (0) {
        } else if (d->conv == kSHORT) {
        } else if (d->conv == kUSHORT) {
        } else if (d->conv == kLONG) {
        //} else if (d->conv == kULONG) {
        } else if (d->conv == kFLOAT) {
        //} else if (d->conv == kDOUBLE) {
        //} else if (d->conv == kBCD) {
        } else {
            errlogPrintf("%s: %s : unsupported conversion \"&%s\" with FTVL = %s\n", __func__, ftvlstr, convstr[d->conv], prec->name);
            prec->pact = 1;
            return -1;
        }
    } else if (prec->ftvl==DBF_LONG || prec->ftvl==DBF_ULONG) {
        if (0) {
        } else if (d->conv == kSHORT) {
        } else if (d->conv == kUSHORT) {
        } else if (d->conv == kLONG) {
        //} else if (d->conv == kULONG) {
        //} else if (d->conv == kFLOAT) {
        //} else if (d->conv == kDOUBLE) {
        //} else if (d->conv == kBCD) {
        } else {
            errlogPrintf("%s: %s : unsupported conversion \"&%s\" with FTVL = %s\n", __func__, ftvlstr, convstr[d->conv], prec->name);
            prec->pact = 1;
            return -1;
        }
    } else if (prec->ftvl==DBF_SHORT || prec->ftvl==DBF_USHORT) {
        if (0) {
        } else if (d->conv == kSHORT) {
        } else if (d->conv == kUSHORT) {
        //} else if (d->conv == kLONG) {
        //} else if (d->conv == kULONG) {
        //} else if (d->conv == kFLOAT) {
        //} else if (d->conv == kDOUBLE) {
        //} else if (d->conv == kBCD) {
        } else {
            prec->pact = 1;
            return -1;
        }
    } else {
        errlogPrintf("%s: %s : unsupported FTVL = %s\n", __func__, prec->name, ftvlstr);
        prec->pact = 1;
        return -1;
    }

    return ret;
}

static long read_waveform(waveformRecord *prec)
{
    //DEBUG
    if (netDevDebug>0) {
        printf("\n%s: %s %s pact=%d\n", __FILE__, __func__, prec->name, prec->pact);
    }

    TRANSACTION *t = prec->dpvt;
    YEW_PLC *d = t->device;

    // make sure that those below are cleared in the event that
    // a multi-step transfer is terminated by an error in the
    // middle of transacton
    d->nleft = 0;
    d->noff = 0;

    //
    long ret = netDevReadWriteXx((dbCommon *)prec);

    return ret;
}

static long config_waveform_command(dbCommon *pxx,
                                    int *option,
                                    uint8_t *buf,
                                    int *len,
                                    void *device,
                                    int transaction_id
                                    )
{
    //DEBUG
    if (netDevDebug>0) {
        printf("\n%s: %s %s pact=%d\n", __FILE__, __func__, pxx->name, pxx->pact);
    }

    waveformRecord *prec = (waveformRecord *)pxx;

    return yew_config_command(buf,
                              len,
                              0, // not used in yew_config_command
                              0, // not used in yew_config_command
                              prec->nelm,
                              option,
                              device
                              );
}

static long parse_waveform_response(dbCommon *pxx,
                                    int *option,
                                    uint8_t *buf,
                                    int *len,
                                    void *device,
                                    int transaction_id
                                    )
{
    waveformRecord *prec = (waveformRecord *)pxx;
    YEW_PLC *d = device;
    const char *ftvlstr = (pamapdbfType[prec->ftvl].strvalue) + 4;

    //DEBUG
    if (netDevDebug>0) {
        printf("\n%s: %s %s pact=%d ftvl=%s conv=%s nleft=%d\n", __FILE__, __func__, prec->name, prec->pact, ftvlstr, convstr[d->conv], d->nleft);
    }

    //
    long ret = 0;

    if (0) {
        //
    } else if (prec->ftvl == DBF_DOUBLE && d->conv == kSHORT) {
        int16_t  *val = d->val;
        double   *bptr = prec->bptr;
        const int start = d->noff / d->width;

        ret = yew_parse_response(buf,
                                 len,
                                 val,
                                 DBF_SHORT,
                                 prec->nelm,
                                 option,
                                 d
                                 );

        const int end = d->noff / d->width;
        for (int i=start; i<end; i++) {
            bptr[i] = val[i];
            //printf("%d %d %f\n", i, val[i], bptr[i]);
        }
    } else if (prec->ftvl == DBF_DOUBLE && d->conv == kUSHORT) {
        uint16_t *val = d->val;
        double   *bptr = prec->bptr;
        const int start = d->noff / d->width;

        ret = yew_parse_response(buf,
                                 len,
                                 val,
                                 DBF_USHORT,
                                 prec->nelm,
                                 option,
                                 d
                                 );

        const int end = d->noff / d->width;
        for (int i=start; i<end; i++) {
            bptr[i] = val[i];
            //printf("%d %u %f\n", i, val[i], bptr[i]);
        }
    } else if (prec->ftvl == DBF_DOUBLE && d->conv == kLONG) {
        int32_t  *val = d->val;
        double   *bptr = prec->bptr;
        const int start = d->noff / d->width;

        ret = yew_parse_response(buf,
                                 len,
                                 val,
                                 DBF_LONG,
                                 prec->nelm,
                                 option,
                                 d
                                 );

        const int end = d->noff / d->width;
        for (int i=start; i<end; i++) {
            bptr[i] = val[i];
            //printf("%d %d %f\n", i, val[i], bptr[i]);
        }
    } else if (prec->ftvl == DBF_DOUBLE && d->conv == kFLOAT) {
        float    *val = d->val;
        double   *bptr = prec->bptr;
        const int start = d->noff / d->width;

        ret = yew_parse_response(buf,
                                 len,
                                 val,
                                 DBF_FLOAT,
                                 prec->nelm,
                                 option,
                                 d
                                 );

        const int end = d->noff / d->width;
        for (int i=start; i<end; i++) {
            bptr[i] = val[i];
            //printf("%d %d %f\n", i, val[i], bptr[i]);
        }
    } else if (prec->ftvl == DBF_FLOAT && d->conv == kSHORT) {
        int16_t  *val = d->val;
        float    *bptr = prec->bptr;
        const int start = d->noff / d->width;

        ret = yew_parse_response(buf,
                                 len,
                                 val,
                                 DBF_SHORT,
                                 prec->nelm,
                                 option,
                                 d
                                 );

        const int end = d->noff / d->width;
        for (int i=start; i<end; i++) {
            bptr[i] = val[i];
            //printf("%d %d %f\n", i, val[i], bptr[i]);
        }
    } else if (prec->ftvl == DBF_FLOAT && d->conv == kUSHORT) {
        uint16_t *val = d->val;
        float    *bptr = prec->bptr;
        const int start = d->noff / d->width;

        ret = yew_parse_response(buf,
                                 len,
                                 val,
                                 DBF_USHORT,
                                 prec->nelm,
                                 option,
                                 d
                                 );

        const int end = d->noff / d->width;
        for (int i=start; i<end; i++) {
            bptr[i] = val[i];
            //printf("%d %u %f\n", i, val[i], bptr[i]);
        }
    } else if (prec->ftvl == DBF_FLOAT && d->conv == kLONG) {
        int32_t  *val = d->val;
        float    *bptr = prec->bptr;
        const int start = d->noff / d->width;

        ret = yew_parse_response(buf,
                                 len,
                                 val,
                                 DBF_LONG,
                                 prec->nelm,
                                 option,
                                 d
                                 );

        const int end = d->noff / d->width;
        for (int i=start; i<end; i++) {
            bptr[i] = val[i];
            //printf("%d %d %f\n", i, val[i], bptr[i]);
        }
    } else if (prec->ftvl == DBF_LONG &&  d->conv == kUSHORT) {
        ret = yew_parse_response(buf,
                                 len,
                                 prec->bptr,
                                 DBF_ULONG,
                                 prec->nelm,
                                 option,
                                 d
                                 );
    } else if (prec->ftvl == DBF_ULONG && d->conv == kSHORT) {
        ret = yew_parse_response(buf,
                                 len,
                                 prec->bptr,
                                 DBF_LONG,
                                 prec->nelm,
                                 option,
                                 d
                                 );
    } else {
        ret = yew_parse_response(buf,
                                 len,
                                 prec->bptr,
                                 prec->ftvl,
                                 prec->nelm,
                                 option,
                                 d
                                 );
    }

    //DEBUG
    if (netDevDebug>0) {
        printf("%s: %s %s : ret=%ld ndata=%d nleft=%d noff=%d\n", __FILE__, __func__, pxx->name, ret, prec->nelm, d->nleft, d->noff);
    }

    switch (ret) {
    case NOT_DONE:
        prec->nord = d->noff;
        // why we don't have break here?
    case 0:
        prec->nord = prec->nelm;
    default:
        ;
    }

    return ret;
}
