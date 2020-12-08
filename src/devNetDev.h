/* devNetDev_314.h */
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
 */

#include <devSup.h>
#include <dbCommon.h>
#ifdef vxWorks
#include <hostLib.h>
#endif

#ifdef vxWorks
typedef int in_addr_t;
#endif

#define MAX_INSTIO_STRING 256

typedef struct {
    long          number;
    DEVSUPFUN     report;
    DEVSUPFUN     init;
    DEVSUPFUN     init_record;
    DEVSUPFUN     get_ioint_info;
    DEVSUPFUN     read_write;
} INTEGERDSET;

typedef struct {
    long          number;
    DEVSUPFUN     report;
    DEVSUPFUN     init;
    DEVSUPFUN     init_record;
    DEVSUPFUN     get_ioint_info;
    DEVSUPFUN     read_write;
    DEVSUPFUN     special_linconv;
} FLOATDSET;

typedef struct {
    long          number;
    DEVSUPFUN     report;
    DEVSUPFUN     init;
    DEVSUPFUN     init_record;
    DEVSUPFUN     get_ioint_info;
    DEVSUPFUN     read_write;
    DEVSUPFUN     setup_attention;
} ATTENTIONDSET;

typedef long (* parse_link_fn_t)(struct link *, struct sockaddr_in *, int *, void *);
typedef long (* config_command_fn_t)(dbCommon *, int *, uint8_t *, int *, void *, int);
typedef long (* parse_response_fn_t)(dbCommon *, int *, uint8_t *, int *, void *, int);
typedef void (* CALLBACK_fn_t)(CALLBACK *);

long         netDevInitXxRecord(struct dbCommon *, struct link *, int, void *,
                                parse_link_fn_t, config_command_fn_t, parse_response_fn_t);
long         netDevReadWriteXx(struct dbCommon *);
long         netDevInfo();
long         netDevInit(void);
long         netDevGetHostId(char *, in_addr_t *);
uint32_t     netDevGetSelfId(void);
long         netDevSetEvMsgLeng(struct dbCommon *, int);
long         parseLinkPlcCommon(struct link *, struct sockaddr_in *,
                            char **, char **, char **, char **, char **, char **);
long         netDevGetIoIntInfo(int, struct dbCommon *, IOSCANPVT *);
long         fromRecordVal(void *, int, void *, int, int, int, int);
long         toRecordVal(void *, int, int, void *, int, int, int);

TRANSACTION *netDevInitInternalIO(struct dbCommon *, struct sockaddr_in,
                                  config_command_fn_t, parse_response_fn_t, CALLBACK_fn_t, void *, int);

int          netDevInternalIO(int, TRANSACTION *, void *, int);
int          netDevDeleteInternalIO(TRANSACTION *);

#define netDevInternalRead(x,y,z)  netDevInternalIO(MPF_READ, (x),(y),(z))
#define netDevInternalWrite(x,y,z) netDevInternalIO(MPF_WRITE,(x),(y),(z))
