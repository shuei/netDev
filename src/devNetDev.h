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

#include <devSup.h>
#include <dbCommon.h>

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

typedef long (* parse_link_fn_t)(DBLINK *, struct sockaddr_in *, uint32_t *, void *);
typedef long (* config_command_fn_t)(dbCommon *, uint32_t *, uint8_t *, int *, void *, int);
typedef long (* parse_response_fn_t)(dbCommon *, uint32_t *, uint8_t *, int *, void *, int);
typedef void (* CALLBACK_fn_t)(CALLBACK *);

long         netDevInitXxRecord(dbCommon *, DBLINK *, uint32_t, double, double, double, void *,
                                parse_link_fn_t, config_command_fn_t, parse_response_fn_t);
long         netDevReadWriteXx(dbCommon *);
long         netDevInfo();
long         netDevInit(int);
long         netDevGetHostId(char *, in_addr_t *);
uint32_t     netDevGetSelfId(void);
long         netDevSetEvMsgLeng(dbCommon *, int);
long         netDevGetIoIntInfo(int, dbCommon *, IOSCANPVT *);

//
uint32_t     netDevBcd2Int(uint16_t bcd, void *precord);
uint16_t     netDevInt2Bcd(int32_t dec, void *precord);
long         parseLinkPlcCommon(DBLINK *, struct sockaddr_in *,
                            char **, char **, char **, char **, char **, char **);
long         fromRecordVal(void *, int, void *, int, int, int, int);
long         toRecordVal(void *, int, int, void *, int, int, int);

//
TRANSACTION *netDevInitInternalIO(dbCommon *, struct sockaddr_in,
                                  config_command_fn_t, parse_response_fn_t, CALLBACK_fn_t, void *, int, double, double, double);
int          netDevInternalIO(int, TRANSACTION *, void *);
int          netDevDeleteInternalIO(TRANSACTION *);

#define netDevInternalRead(t,d,s,r,e)  netDevInternalIO(MPF_READ, (t),(d))
#define netDevInternalWrite(t,d,s,r,e) netDevInternalIO(MPF_WRITE,(t),(d))
