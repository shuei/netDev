/* drvNetMpf_314.h */
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#ifdef vxWorks
#include <sockLib.h>
#endif

#include <errlog.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsTimer.h>
#include <ellLib.h>
#include <callback.h>
#include <dbScan.h>
#include <assert.h>
#include <dbCommon.h>

#define PROTO_MASK 0x80000000
#define EVENT_MASK 0x40000000
#define OMRON_MASK 0x20000000
#define DIREC_MASK 0x10000000
#define RECON_MASK 0x04000000
#define RETRY_MASK 0x02000000
#define QUEUE_MASK 0x01000000
#define BSIZE_MASK 0x00ff0000
#define BSIZE_SIFT 16
#define FINET_MASK 0x00008000
#define TMOUT_MASK 0x00007fff
#define TMOUT_SIFT 0
#define MPF_OPT_MASK ( PROTO_MASK | EVENT_MASK | OMRON_MASK | BSIZE_MASK | RETRY_MASK)

#define GET_PROTOCOL(x)    ((x) & PROTO_MASK)
#define GET_EVENT(x)       ((x) & EVENT_MASK)
#define GET_DIRECTION(x)   ((x) & DIREC_MASK)
#define GET_QUEUE(x)       ((x) & QUEUE_MASK)
#define GET_RECON(x)       ((x) & RECON_MASK)
#define GET_RETRY(x)       ((x) & RETRY_MASK)
#define GET_SAMEPORT(x)    ((x) & OMRON_MASK)
#define GET_BUFSIZE(x)    (((x) & BSIZE_MASK) >> BSIZE_SIFT)
#define GET_FINETMO(x)     ((x) & FINET_MASK)
#define GET_TIMEOUT(x)    (((x) & TMOUT_MASK) >> TMOUT_SIFT)
#define GET_MPF_OPTION(x)  ((x) & MPF_OPT_MASK)

#define MPF_UDP      0
#define MPF_TCP      PROTO_MASK
#define MPF_NORMAL   0
#define MPF_EVENT    EVENT_MASK
#define MPF_ANYPORT  0
#define MPF_SAMEPORT OMRON_MASK
#define MPF_READ     0
#define MPF_WRITE    DIREC_MASK
#define MPF_LAST     0
#define MPF_FIRST    QUEUE_MASK
#define MPF_STATIC   0
#define MPF_RECONN   RECON_MASK
#define MPF_LINGER   0
#define MPF_NOLINGER RETRY_MASK
#define MPF_NORMTMO  0
#define MPF_FINETMO  FINET_MASK

#define isUdp(x)        (GET_PROTOCOL(x) == MPF_UDP)
#define isTcp(x)        (GET_PROTOCOL(x) == MPF_TCP)
#define setUdp(x)       ((x) &= ~PROTO_MASK)
#define setTcp(x)       ((x) |= PROTO_MASK)
#define isNormal(x)     (GET_EVENT(x) == MPF_NORMAL)
#define isEvent(x)      (GET_EVENT(x) == MPF_EVENT)
#define isOthers(x)     (GET_SAMEPORT(x) == MPF_ANYPORT)
#define isOmron(x)      (GET_SAMEPORT(x) == MPF_SAMEPORT)
#define isRead(x)       (GET_DIRECTION(x) == MPF_READ)
#define isWrite(x)      (GET_DIRECTION(x) == MPF_WRITE)
#define isLast(x)       (GET_QUEUE(x) == MPF_LAST)
#define isFirst(x)      (GET_QUEUE(x) == MPF_FIRST)
#define setLast(x)      ((x) &= ~QUEUE_MASK)
#define setFirst(x)     ((x) |= QUEUE_MASK)
#define isStatic(x)     (GET_RECON(x) == MPF_STATIC)
#define isReconn(x)     (GET_RECON(x) == MPF_RECONN)
#define setStatic(x)    ((x) &= ~RECON_MASK)
#define setReconn(x)    ((x) |= RECON_MASK)
#define isLinger(x)     (GET_RETRY(x) == MPF_LINGER)
#define isNoLinger(x)   (GET_RETRY(x) == MPF_NOLINGER)
#define isFineTmo(x)    (GET_FINETMO(x) == MPF_FINETMO)

#ifndef vxWorks
#define TRUE         1
#define FALSE        0
#define OK         ((long)  0)
#define ERROR      ((long) -1)
#endif

#define NOT_MINE   (ERROR - 1)
#define NOT_DONE   (NOT_MINE - 1)
#define RCV_MORE   (NOT_DONE - 1)
#define NO_RESP    (RCV_MORE - 1)
#define SEND_BUF_SIZE(x) (4096 * (GET_BUFSIZE(x) + 1))
#define RECV_BUF_SIZE(x) (4096 * (GET_BUFSIZE(x) + 1))
#define SEND_PRIORITY         50
#define SEND_STACK            (epicsThreadGetStackSize(epicsThreadStackBig))
#define RECV_PRIORITY         50
#define RECV_STACK            (epicsThreadGetStackSize(epicsThreadStackBig))
#define TCP_PARENT_PRIORITY   50
#define TCP_PARENT_STACK      (epicsThreadGetStackSize(epicsThreadStackBig))
#define EVSRVR_PRIORITY       50
#define EVSRVR_STACK          (epicsThreadGetStackSize(epicsThreadStackBig))
#define NUM_MPF_VAR           256

typedef struct {
    double             timeout;
    int                timeout_flag;
    CALLBACK           callback;
    uint32_t           send_counter;
    uint32_t           cancel_counter;
    uint32_t           timeout_counter;
    uint32_t           receive_counter;
} ASYNC_IO;

typedef struct {
    IOSCANPVT          ioscanpvt;
    struct sockaddr_in client_addr;
    int                ev_msg_len;
} IO_EVENT;

typedef struct {
    ELLNODE            node;
    dbCommon          *record;
    void              *facility;
    int                option;
    int                transaction_id;
    void              *device;
    int                ret;
    long             (*config_command)();
    long             (*parse_response)();
    union {
        ASYNC_IO async;
        IO_EVENT event;
    } io;
    struct sockaddr_in peer_addr; // for OMRON ETN01 support
} TRANSACTION;

typedef struct {
    ELLNODE            node;
    int                id;
    int                option;
    int                sfd;
    struct sockaddr_in peer_addr;
    struct sockaddr_in sender_addr;
    uint32_t           counter;
    int                show_msg;
    ssize_t            send_len;
    ssize_t            recv_len;
    void              *send_buf;
    void              *recv_buf;
    int                mpf_var[NUM_MPF_VAR];
} MPF_COMMON;

typedef struct {
    MPF_COMMON         mpf;
    TRANSACTION       *in_transaction;
    epicsMutexId       in_t_mutex;
    ELLLIST            reqQueue;
    epicsMutexId       reqQ_mutex;
    epicsEventId       req_queued;
    epicsEventId       next_cycle;
    epicsTimerId       wd_id;
    epicsThreadId      send_tid;
    epicsThreadId      recv_tid;
    int                tmo_event;
    int                event_num;
    epicsTimeStamp     send_time;
    epicsTimeStamp     recv_time;
} PEER;

typedef struct {
    MPF_COMMON         mpf;
    unsigned short     port;
    ELLLIST            eventQueue;
    epicsMutexId       eventQ_mutex;
    epicsThreadId      server_tid;
    void              *parent;
} SERVER;

typedef struct {
    ELLNODE            node;
    struct sockaddr_in peer_addr;
    int                option;
    int                is_bin;
} MSG_BY_IP;

typedef struct {
    ELLNODE            node;
    dbCommon          *precord;
} MSG_BY_PV;

typedef struct {
    ELLNODE            node;
    struct sockaddr_in peer_addr;
} RTT_ITEM;

//
#define GET_PEER_INET_ADDR(t)    (((PEER *)((TRANSACTION *)(t))->facility)->mpf.peer_addr.sin_addr.s_addr)
#define GET_PEER_ID(t)           (((PEER *)((TRANSACTION *)(t))->facility)->mpf.id)
#define GET_DPVT(p)              (((dbCommon *)(p))->dpvt)
#define MPF_OPTION(t)            ( ((PEER *)((TRANSACTION *)(t))->facility)->mpf.option)
#define MPF_OPTION_PTR(t)        (&((PEER *)((TRANSACTION *)(t))->facility)->mpf.option)
#define MPF_VAR_PTR(t)           (&((PEER *)((TRANSACTION *)(t))->facility)->mpf.mpf_var[0])
#define GET_PEER_DEVICE(t)       (((TRANSACTION *)(t))->device)
#define GET_CLIENT_INET_ADDR(t)  (((TRANSACTION *)(t))->io.event.client_addr.sin_addr.s_addr)
#define GET_SERVER_PORT(t)       (((SERVER *)((TRANSACTION *)(t))->facility)->port)
#define GET_SERVER_ID(t)         (((SERVER *)((TRANSACTION *)(t))->facility)->mpf.id)

//
long drvNetMpfSendRequest(TRANSACTION *);
PEER *drvNetMpfInitPeer(struct sockaddr_in, int);
SERVER *drvNetMpfInitServer(unsigned short, int);
long drvNetMpfRegisterEvent(TRANSACTION *);

//
#define TICK_PER_SECOND  100

//
#define SANITY_CHECK
//#define SANITY_PRINT
//#define DEBUG

#ifdef DEBUG
#define LOGMSG(...) errlogPrintf(__VA_ARGS__)
#define DEFAULT_TIMEOUT  (TICK_PER_SECOND | MPF_FINETMO)
#else
#define LOGMSG(...)
#define DEFAULT_TIMEOUT  (TICK_PER_SECOND | MPF_FINETMO)
#endif

#define SHOW_MESSAGE
#ifdef SHOW_MESSAGE
#define DUMP_MSG(arg0, arg1, arg2, arg3) {if ( ((MPF_COMMON *)(arg0))->show_msg ) \
dump_msg( (arg1), (arg2), (arg3), ((MPF_COMMON *)(arg0))->show_msg );}
#else
#define DUMP_MSG(arg0, arg1, arg2, arg3)
#endif
