/* devWaveformChinoLogL.c */
/* Author : Makoto Tobiyama 19/Jul/2005 */
/* Modification Log:
 * -----------------
 */

#include <waveformRecord.h>

/***************************************************************
 * Waveform (command/response IO)
 ***************************************************************/
LOCAL long init_waveform_record(struct waveformRecord *);
LOCAL long read_waveform(struct waveformRecord *);
LOCAL long config_waveform_command(struct dbCommon *, int *, uint8_t *, int *, void *, int);
LOCAL long parse_waveform_response(struct dbCommon *, int *, uint8_t *, int *, void *, int);

INTEGERDSET devWfChinoLogL = {
    5,
    NULL,
    netDevInit,
    init_waveform_record,
    NULL,
    read_waveform
};

epicsExportAddress(dset, devWfChinoLogL);

LOCAL long init_waveform_record(struct waveformRecord *pwf)
{
    CHINOL_LOG *d  = chino_calloc(0, 0, 0, 0, 2);

    return netDevInitXxRecord((struct dbCommon *) pwf,
                              &pwf->inp,
                              MPF_READ | CHINOL_GET_PROTO | DEFAULT_TIMEOUT,
                              d,
                              chino_parse_link,
                              config_waveform_command,
                              parse_waveform_response
                              );
}

LOCAL long read_waveform(struct waveformRecord *pwf)
{
//  TRANSACTION *t = (TRANSACTION *) pwf->dpvt;
//  CHINOL_LOG *d = (CHINOL_LOG *) t->device;

    /*
     * make sure that those below are cleared in the event that
     * a multi-step transfer is terminated by an error in the
     * middle of transacton
     */

    return netDevReadWriteXx((struct dbCommon *) pwf);
}

LOCAL long config_waveform_command(struct dbCommon *pxx,
                                   int *option,
                                   uint8_t *buf,
                                   int *len,
                                   void *device,
                                   int transaction_id
                                   )
{
    struct waveformRecord *pwaveform = (struct waveformRecord *)pxx;
    CHINOL_LOG *d = (CHINOL_LOG *) device;

    return chino_config_command(buf,
                                len,
                                pwaveform->bptr,
                                pwaveform->ftvl,
                                pwaveform->nelm,
                                option,
                                d,
                                transaction_id
                                );
}

LOCAL long parse_waveform_response(struct dbCommon *pxx,
                                   int *option,
                                   uint8_t *buf,
                                   int *len,
                                   void *device,
                                   int transaction_id
                                   )
{
    struct waveformRecord *pwaveform = (struct waveformRecord *)pxx;
    CHINOL_LOG *d = (CHINOL_LOG *) device;

    long ret = chino_parse_response(buf,
                                    len,
                                    pwaveform->bptr,
                                    pwaveform->ftvl,
                                    pwaveform->nelm,
                                    option,
                                    d,
                                    transaction_id
                                    );

    switch (ret) {
    case 0:
        pwaveform->nord = pwaveform->nelm;
        break;
    default:
        // never reach here as chino_parse_response() always returns zero.
        ;
    }

    return ret;
}
