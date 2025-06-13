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
            errlogPrintf("%s: %s : unsupported conversion \"&%s\" with FTVL = %s\n", __func__, ftvlstr, convstr[d->conv], prec->name);
            prec->pact = 1;
            return -1;
        }

    } else {
        errlogPrintf("%s: %s : unsupported FTVL %s\n", __func__, prec->name, ftvlstr);
        prec->pact = 1;
        return -1;
    }

    return ret;
}

static long write_arrayout(arrayoutRecord *prec)
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
    arrayoutRecord *prec = (arrayoutRecord *)pxx;
    YEW_PLC *d = device;
    const char *ftvlstr = (pamapdbfType[prec->ftvl].strvalue) + 4;

    //DEBUG
    if (netDevDebug>0) {
        printf("\n%s: %s %s pact=%d ftvl=%s conv=%s nleft=%d\n", __FILE__, __func__, prec->name, prec->pact, ftvlstr, convstr[d->conv], d->nleft);
    }

    if (0) {
        //
    } else if (prec->ftvl == DBF_DOUBLE && (d->conv == kSHORT || d->conv == kUSHORT)) {
        int16_t *val = d->val;
        if (d->nleft == 0 /* && d->noff == 0 */) {
            double *bptr = prec->bptr;
            for (int i=0; i<prec->nelm; i++) {
                val[i] = bptr[i];
                //printf("%d %d %f\n", i, val[i], bptr[i]);
            }
        }

        return yew_config_command(buf,
                                  len,
                                  val,
                                  DBF_SHORT,
                                  prec->nelm,
                                  option,
                                  device
                                  );
    } else if (prec->ftvl == DBF_DOUBLE && d->conv == kLONG) {
        int32_t *val = d->val;
        if (d->nleft == 0 /* && d->noff == 0 */) {
            double *bptr = prec->bptr;
            for (int i=0; i<prec->nelm; i++) {
                val[i] = bptr[i];
                //printf("%d %d %f\n", i, val[i], bptr[i]);
            }
        }

        return yew_config_command(buf,
                                  len,
                                  val,
                                  DBF_LONG,
                                  prec->nelm,
                                  option,
                                  device
                                  );
    } else if (prec->ftvl == DBF_DOUBLE && d->conv == kFLOAT) {
        float *val = d->val;
        if (d->nleft == 0 /* && d->noff == 0 */) {
            double *bptr = prec->bptr;
            for (int i=0; i<prec->nelm; i++) {
                val[i] = bptr[i];
                //printf("%d %d %f\n", i, val[i], bptr[i]);
            }
        }

        return yew_config_command(buf,
                                  len,
                                  val,
                                  DBF_FLOAT,
                                  prec->nelm,
                                  option,
                                  device
                                  );
    } else if (prec->ftvl == DBF_FLOAT && (d->conv == kSHORT || d->conv == kUSHORT)) {
        int16_t *val = d->val;
        if (d->nleft == 0 /* && d->noff == 0 */) {
            float *bptr = prec->bptr;
            for (int i=0; i<prec->nelm; i++) {
                val[i] = bptr[i];
                //printf("%d %d %f\n", i, val[i], bptr[i]);
            }
        }

        return yew_config_command(buf,
                                  len,
                                  val,
                                  DBF_SHORT,
                                  prec->nelm,
                                  option,
                                  device
                                  );
    } else if (prec->ftvl == DBF_FLOAT && d->conv == kLONG) {
        int32_t *val = d->val;
        if (d->nleft == 0 /* && d->noff == 0 */) {
            float *bptr = prec->bptr;
            for (int i=0; i<prec->nelm; i++) {
                val[i] = bptr[i];
                //printf("%d %d %f\n", i, val[i], bptr[i]);
            }
        }

        return yew_config_command(buf,
                                  len,
                                  val,
                                  DBF_LONG,
                                  prec->nelm,
                                  option,
                                  device
                                  );
#if 0
    } else if ((prec->ftvl == DBF_LONG||prec->ftvl == DBF_ULONG) && d->conv == kUSHORT) {
        uint16_t *val = d->val;
        if (d->nleft == 0 /* && d->noff == 0 */) {
            uint32_t *bptr = prec->bptr;
            for (int i=0; i<prec->nelm; i++) {
                val[i] = bptr[i];
                //printf("%d %d %f\n", i, val[i], bptr[i]);
            }
        }

        return yew_config_command(buf,
                                  len,
                                  prec->bptr,
                                  DBF_USHORT,
                                  prec->nelm,
                                  option,
                                  device
                                  );
    } else if ((prec->ftvl == DBF_LONG||prec->ftvl == DBF_ULONG) && d->conv == kSHORT) {
        int16_t *val = d->val;
        if (d->nleft == 0 /* && d->noff == 0 */) {
            uint32_t *bptr = prec->bptr;
            for (int i=0; i<prec->nelm; i++) {
                val[i] = bptr[i];
                //printf("%d %d %f\n", i, val[i], bptr[i]);
            }
        }

        return yew_config_command(buf,
                                  len,
                                  prec->bptr,
                                  DBF_SHORT,
                                  prec->nelm,
                                  option,
                                  device
                                  );
#endif
    } else {
        return yew_config_command(buf,
                                  len,
                                  prec->bptr,
                                  prec->ftvl,
                                  prec->nelm,
                                  option,
                                  d
                                  );
    }
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
        printf("\n%s: %s %s pact=%d\n", __FILE__, __func__, pxx->name, pxx->pact);
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
