/* drvNetMpf.c */
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

#include <errno.h>
#include <string.h>

#include <dbAccess.h>
#include <dbAddr.h>
#include <dbCommon.h>
#include <epicsThread.h>
#include <recSup.h>
#include <drvSup.h>
#include <iocsh.h>
#include <epicsExport.h>

#include "drvNetMpf.h"

typedef enum {
    RECV = 0,
    SEND = 1,
} DIRECTION;

static int init_flag;
static int globalPeerCount;
static int globalServerCount;
static int event_server_start_flag;
static epicsTimerQueueId timerQueue;

static long create_semaphores(PEER *);
static long prepare_udp_server_socket(SERVER *);
static long spawn_io_tasks(PEER *);
static long spawn_server_task(SERVER *);
static void delete_peer(PEER *);
static void delete_server(SERVER *);
static void remove_server(SERVER *);
static TRANSACTION *get_request_from_queue(PEER *);
static int send_msg(MPF_COMMON *);
static int recv_msg(MPF_COMMON *);
static void do_connect(PEER *);
static void send_task(PEER *);
static void recv_task(PEER *);
static void mpf_timeout_handler(PEER *);
static long init(void);
static long report(void);
static long spawn_tcp_parent(SERVER *);
static int tcp_parent(SERVER *);
static int event_server(SERVER *);
void dump_msg(uint8_t *, ssize_t, DIRECTION, bool);
void startEventServer(const iocshArgBuf *);
void mpfHelp(const iocshArgBuf *);
void peerShow(const iocshArgBuf *);
void peerShowAll(const iocshArgBuf *);
void serverShow(const iocshArgBuf *);
void serverShowAll(const iocshArgBuf *);
void showMsg(const iocshArgBuf *);
void stopMsg(const iocshArgBuf *);
void showEventMsg(const iocshArgBuf *);
void stopEventMsg(const iocshArgBuf *);
void setTmoEventNum(const iocshArgBuf *);
void enableTmoEvent(const iocshArgBuf *);
void disableTmoEvent(const iocshArgBuf *);
void showmsg(const iocshArgBuf *);
void stopmsg(const iocshArgBuf *);
void do_showmsg(MPF_COMMON *, uint8_t *, int, DIRECTION);
void showio(const iocshArgBuf *);
void stopio(const iocshArgBuf *);
void do_showio(TRANSACTION *, int);
void showrtt(const iocshArgBuf *);
void stoprtt(const iocshArgBuf *);
void do_showrtt(PEER *);

long netDevGetHostId(char *hostname, in_addr_t *hostid);

// error code that send_msg()/recv_msg() may return
#define RECONNECT (ERROR - 1)

// Not yet in use
//#define MAX_CONNECT_RETRY  1

//
struct {
    long      number;
    DRVSUPFUN report;
    DRVSUPFUN init;
} drvNetMpf = {
    2L,
    report,
    init,
};

epicsExportAddress(drvet, drvNetMpf);

static struct {
    epicsMutexId  mutex;
    ELLLIST list;
} peerList;

static struct {
    epicsMutexId  mutex;
    ELLLIST list;
} serverList;

static struct {
    epicsMutexId  mutex;
    ELLLIST list;
} showmsgList;

static struct {
    epicsMutexId  mutex;
    ELLLIST list;
} showioList;

static struct {
    epicsMutexId  mutex;
    ELLLIST list;
} showrttList;

static const iocshArg showMsgArg0 = {"id", iocshArgInt};
static const iocshArg showMsgArg1 = {"option", iocshArgInt};
static const iocshArg * const showMsgArgs[2] = {&showMsgArg0, &showMsgArg1};
static const iocshFuncDef showMsgDef = {"showMsg", 2, showMsgArgs};

static const iocshArg stopMsgArg0 = {"id", iocshArgInt};
static const iocshArg * const stopMsgArgs[1] = {&stopMsgArg0};
static const iocshFuncDef stopMsgDef = {"stopMsg", 1, stopMsgArgs};

static const iocshFuncDef peerShowAllDef = {"peerShowAll", 0, NULL};

static const iocshArg peerShowArg0 = {"id", iocshArgInt};
static const iocshArg peerShowArg1 = {"level", iocshArgInt};
static const iocshArg * const peerShowArgs[2] = {&peerShowArg0, &peerShowArg1};
static const iocshFuncDef peerShowDef = {"peerShow", 2, peerShowArgs};

static const iocshArg showEventMsgArg0 = {"id", iocshArgInt};
static const iocshArg showEventMsgArg1 = {"option", iocshArgInt};
static const iocshArg * const showEventMsgArgs[2] = {&showEventMsgArg0, &showEventMsgArg1};
static const iocshFuncDef showEventMsgDef = {"showEventMsg", 2, showEventMsgArgs};

static const iocshArg stopEventMsgArg0 = {"id", iocshArgInt};
static const iocshArg * const stopEventMsgArgs[1] = {&stopEventMsgArg0};
static const iocshFuncDef stopEventMsgDef = {"stopEventMsg", 1, stopEventMsgArgs};

static const iocshFuncDef serverShowAllDef = {"serverShowAll", 0, NULL};

static const iocshArg serverShowArg0 = {"id", iocshArgInt};
static const iocshArg serverShowArg1 = {"level", iocshArgInt};
static const iocshArg * const serverShowArgs[2] = {&serverShowArg0, &serverShowArg1};
static const iocshFuncDef serverShowDef = {"serverShow", 2, serverShowArgs};

static const iocshFuncDef startEventServerDef = {"startEventServer", 0, NULL};

static const iocshArg setTmoEventNumArg0 = {"ip", iocshArgString};
static const iocshArg setTmoEventNumArg1 = {"port", iocshArgInt};
static const iocshArg setTmoEventNumArg2 = {"proto", iocshArgString};
static const iocshArg setTmoEventNumArg3 = {"num", iocshArgInt};
static const iocshArg * const setTmoEventNumArgs[4] = {&setTmoEventNumArg0, &setTmoEventNumArg1,
                                                       &setTmoEventNumArg2, &setTmoEventNumArg3};
static const iocshFuncDef setTmoEventNumDef = {"setTmoEventNum", 4, setTmoEventNumArgs};

static const iocshArg enableTmoEventArg0 = {"ip", iocshArgString};
static const iocshArg enableTmoEventArg1 = {"port", iocshArgInt};
static const iocshArg enableTmoEventArg2 = {"proto", iocshArgString};
static const iocshArg * const enableTmoEventArgs[3] = {&enableTmoEventArg0, &enableTmoEventArg1,
                                                       &enableTmoEventArg2};
static const iocshFuncDef enableTmoEventDef = {"enableTmoEvent", 3, enableTmoEventArgs};

static const iocshArg disableTmoEventArg0 = {"ip", iocshArgString};
static const iocshArg disableTmoEventArg1 = {"port", iocshArgInt};
static const iocshArg disableTmoEventArg2 = {"proto", iocshArgString};
static const iocshArg * const disableTmoEventArgs[3] = {&disableTmoEventArg0, &disableTmoEventArg1,
                                                        &disableTmoEventArg2};
static const iocshFuncDef disableTmoEventDef = {"disableTmoEvent", 3, disableTmoEventArgs};

static const iocshArg showmsgArg0 = {"ip", iocshArgString};
static const iocshArg showmsgArg1 = {"option", iocshArgInt};
static const iocshArg * const showmsgArgs[2] = {&showmsgArg0, &showmsgArg1};
static const iocshFuncDef showmsgDef = {"showmsg", 2, showmsgArgs};

static const iocshFuncDef stopmsgDef = {"stopmsg", 0, NULL};

static const iocshArg showioArg0 = {"pv", iocshArgString};
static const iocshArg * const showioArgs[1] = {&showioArg0};
static const iocshFuncDef showioDef = {"showio", 1, showioArgs};

static const iocshFuncDef stopioDef = {"stopio", 0, NULL};

static const iocshArg showrttArg0 = {"ip", iocshArgString};
static const iocshArg * const showrttArgs[2] = {&showrttArg0};
static const iocshFuncDef showrttDef = {"showrtt", 1, showrttArgs};

static const iocshFuncDef stoprttDef = {"stoprtt", 0, NULL};

//
// Report
//
static long report(void)
{
    printf("drvNetMpf: report() has currently nothing to do.\n");

    return OK;
}

//
// Init
//
static long init(void)
{
    LOGMSG("drvNetMpf: init() entered\n");

    if (init_flag) {
        return OK;
    }
    init_flag = 1;

    if ((peerList.mutex = epicsMutexCreate()) == 0) {
        errlogPrintf("drvNetMpf: %s: epicsMutexCreate(peerList) failed\n", __func__);
        return ERROR;
    }

    if ((serverList.mutex = epicsMutexCreate()) == 0) {
        errlogPrintf("drvNetMpf: %s: epicsMutexCreate(serverList) failed\n", __func__);
        return ERROR;
    }

    if ((showmsgList.mutex = epicsMutexCreate()) == 0) {
        errlogPrintf("drvNetMpf: %s: epicsMutexCreate(showmsgList) failed\n", __func__);
        return ERROR;
    }

    if ((showioList.mutex = epicsMutexCreate()) == 0) {
        errlogPrintf("drvNetMpf: %s: epicsMutexCreate(showioList) failed\n", __func__);
        return ERROR;
    }

    if ((showrttList.mutex = epicsMutexCreate()) == 0) {
        errlogPrintf("drvNetMpf: %s: epicsMutexCreate(showrttList) failed\n", __func__);
        return ERROR;
    }

    ellInit(&peerList.list);
    ellInit(&serverList.list);
    ellInit(&showmsgList.list);
    ellInit(&showioList.list);
    ellInit(&showrttList.list);

    if (!(timerQueue = epicsTimerQueueAllocate(1, epicsThreadPriorityScanHigh))) {
        errlogPrintf("drvNetMpf: %s: epicsTimerQueueAllocate failed\n", __func__);
        return ERROR;
    }

    iocshRegister(&showMsgDef, showMsg);
    iocshRegister(&stopMsgDef, stopMsg);
    iocshRegister(&peerShowAllDef, peerShowAll);
    iocshRegister(&peerShowDef, peerShow);
    iocshRegister(&setTmoEventNumDef, setTmoEventNum);
    iocshRegister(&enableTmoEventDef, enableTmoEvent);
    iocshRegister(&disableTmoEventDef, disableTmoEvent);

    iocshRegister(&showEventMsgDef, showEventMsg);
    iocshRegister(&stopEventMsgDef, stopEventMsg);
    iocshRegister(&serverShowAllDef, serverShowAll);
    iocshRegister(&serverShowDef, serverShow);
    iocshRegister(&startEventServerDef, startEventServer);

    iocshRegister(&showmsgDef, showmsg);
    iocshRegister(&stopmsgDef, stopmsg);
    iocshRegister(&showioDef, showio);
    iocshRegister(&stopioDef, stopio);
    iocshRegister(&showrttDef, showrtt);
    iocshRegister(&stoprttDef, stoprtt);

    return OK;
}

//
// Create semaphores
//
static long create_semaphores(PEER *p)
{
    LOGMSG("drvNetMpf: create_semaphores(%8p)\n", p);

    if ((p->in_t_mutex=epicsMutexCreate()) == 0) {
        errlogPrintf("drvNetMpf: %s: epicsMutexCreate(in_t) failed\n", __func__);
        return ERROR;
    }

    if ((p->reqQ_mutex=epicsMutexCreate()) == 0) {
        errlogPrintf("drvNetMpf: %s: epicsMutexCreate(reqQ) failed\n", __func__);
        return ERROR;
    }

    if ((p->req_queued=epicsEventCreate(epicsEventEmpty)) == 0) {
        errlogPrintf("drvNetMpf: %s: epicsEventCreate(req_queued) failed\n", __func__);
        return ERROR;
    }

    if ((p->next_cmd=epicsEventCreate(epicsEventEmpty)) == 0) {
        errlogPrintf("drvNetMpf: %s: epicsEventCreate(next_cmd) failed\n", __func__);
        return ERROR;
    }

    if ((p->next_rsp=epicsEventCreate(epicsEventEmpty)) == 0) {
        errlogPrintf("drvNetMpf: %s: epicsEventCreate(next_rsp) failed\n", __func__);
        return ERROR;
    }

    return OK;
}

//
// Spawn I/O tasks
//
static long spawn_io_tasks(PEER *p)
{
    char *send_t_name = "netDevSendTask";
    char *recv_t_name = "netDevRecvTask";
    char  task_name[32];

    LOGMSG("drvNetMpf: spawn_io_tasks(%8p)\n", p);

    // Open a socket
    do_connect(p);

    //
    sprintf(task_name, "%s_%d", send_t_name, p->mpf.id);
    if ((p->send_tid = epicsThreadCreate(task_name,
                                         SEND_PRIORITY,
                                         SEND_STACK,
                                         (EPICSTHREADFUNC)send_task,
                                         p
                                         )) == 0) {
        p->send_tid = 0;
        errlogPrintf("drvNetMpf: %s: epicsThreadCreate(send_task) failed\n", __func__);
        return ERROR;
    }

    //
    sprintf(task_name, "%s_%d", recv_t_name, p->mpf.id);
    if ((p->recv_tid = epicsThreadCreate(task_name,
                                         RECV_PRIORITY,
                                         RECV_STACK,
                                         (EPICSTHREADFUNC)recv_task,
                                         p
                                         )) == 0) {
        p->recv_tid = 0;
        errlogPrintf("drvNetMpf: %s: epicsThreadCreate(recv_task) failed\n", __func__);
        return ERROR;
    }

    return OK;
}

//
// Delete peer
// This function should be called only before
// the peer is added to the peerList.
//
static void delete_peer(PEER *p)
{
    LOGMSG("drvNetMpf: delete_peer(%8p)\n", p);

    if (p->mpf.sfd) {
        close(p->mpf.sfd);
    }

    //if (p->wd_id) epicsTimerDestroy(p->wd_id);

    if (p->next_rsp) {
        epicsEventDestroy(p->next_rsp);
    }
    if (p->next_cmd) {
        epicsEventDestroy(p->next_cmd);
    }
    if (p->req_queued) {
        epicsEventDestroy(p->req_queued);
    }
    if (p->reqQ_mutex) {
        epicsMutexDestroy(p->reqQ_mutex);
    }
    if (p->mpf.send_buf) {
        free(p->mpf.send_buf);
    }
    if (p->mpf.recv_buf) {
        free(p->mpf.recv_buf);
    }

    free(p);
}

//
// Create and initialize peer structure
//
PEER *drvNetMpfInitPeer(struct sockaddr_in peer_addr,
                        uint32_t option,
                        double send_timeout,
                        double recv_timeout,
                        double epicsTimer_timeout // for backward compatibility
                        )
{
    if (!init_flag) {
        errlogPrintf("drvNetMpf: %s: not initialized, check xxxAppInclude.dbd\n", __func__);
        return NULL;
    }

    PEER *p;
    epicsMutexMustLock(peerList.mutex);
    {
        for (p = (PEER *)ellFirst(&peerList.list); p; p = (PEER *)ellNext(&p->mpf.node)) {
            if ((p->mpf.peer_addr.sin_addr.s_addr == peer_addr.sin_addr.s_addr || isOmron(option)) &&
                p->mpf.peer_addr.sin_port == peer_addr.sin_port &&
                GET_PROTOCOL(p->mpf.option) == GET_PROTOCOL(option)) {
                break;
            }
        }

        if (p) {
            if (GET_MPF_OPTION(p->mpf.option) != GET_MPF_OPTION(option)) {
                errlogPrintf("drvNetMpf: %s: peer option does not match\n", __func__);
                epicsMutexUnlock(peerList.mutex);
                return NULL;
            }

            epicsMutexUnlock(peerList.mutex);
            return p;
        }

        p = calloc(1, sizeof(PEER));

        if (!p) {
            errlogPrintf("drvNetMpf: %s: calloc failed\n", __func__);
            epicsMutexUnlock(peerList.mutex);
            return NULL;
        }

        p->mpf.id = globalPeerCount++;
        p->mpf.peer_addr = peer_addr;
        p->mpf.peer_addr.sin_family = AF_INET;

        LOGMSG("drvNetMpf: drvNetMpfInitPeer(0x%08x,0x%08x,0x%08x)\n",
               ntohl(p->mpf.peer_addr.sin_addr.s_addr),
               ntohl(p->mpf.peer_addr.sin_port),
               ntohs(p->mpf.peer_addr.sin_family));

        p->mpf.option = GET_MPF_OPTION(option);
        ellInit(&p->reqQueue);

        if (create_semaphores(p) == ERROR) {
            errlogPrintf("drvNetMpf: %s: create_semaphores failed\n", __func__);
            delete_peer(p);
            epicsMutexUnlock(peerList.mutex);
            return NULL;
        }

        if (!(p->wd_id = epicsTimerQueueCreateTimer(timerQueue,
                                                    (epicsTimerCallback)mpf_timeout_handler,
                                                    p
                                                    ))) {
            errlogPrintf("drvNetMpf: %s: epicsTimerQueueCreateTimer failed\n", __func__);
            delete_peer(p);
            epicsMutexUnlock(peerList.mutex);
            return NULL;
        }

        //Debug
        //errlogPrintf("%s: %s getbufsiz=%d bufsiz=%d\n", __FILE__, __func__, GET_BUFSIZE(option), SEND_BUF_SIZE(option));

        LOGMSG("drvNetMpf: send buf size %d\n", SEND_BUF_SIZE(p->mpf.option));
        LOGMSG("drvNetMpf: recv buf size %d\n", RECV_BUF_SIZE(p->mpf.option));

        p->mpf.send_buf = calloc(1, SEND_BUF_SIZE(p->mpf.option));
        if (!p->mpf.send_buf) {
            errlogPrintf("drvNetMpf: %s: calloc failed (send_buf)\n", __func__);
            delete_peer(p);
            epicsMutexUnlock(serverList.mutex);
            return NULL;
        }

        p->mpf.recv_buf = calloc(1, RECV_BUF_SIZE(p->mpf.option));
        if (!p->mpf.recv_buf) {
            errlogPrintf("drvNetMpf: %s: calloc failed (recv_buf)\n", __func__);
            delete_peer(p);
            epicsMutexUnlock(serverList.mutex);
            return NULL;
        }


        //
        if ((send_timeout>0 || recv_timeout>0) && epicsTimer_timeout>0) {
            errlogPrintf("drvNetMpf: send/recv timeout takes precedence over epicsTimer timeout\n");
            epicsTimer_timeout = 0;
        }
        p->send_timeout = send_timeout;
        p->recv_timeout = recv_timeout;
        p->epicsTimer_timeout = epicsTimer_timeout;

        if (p->send_timeout>0 || p->recv_timeout>0) {
            errlogPrintf("drvNetMpf: %s: setting send timeout to %.6f\n",  __func__, p->send_timeout);
            errlogPrintf("drvNetMpf: %s: setting recv timeout to %.6lf\n", __func__, p->recv_timeout);
        } else {
            errlogPrintf("drvNetMpf: %s: setting epicsTimer timeout to %.6lf\n", __func__, p->epicsTimer_timeout);
        }

        //
        if (spawn_io_tasks(p) == ERROR) {
            errlogPrintf("drvNetMpf: %s: spawn_io_tasks failed\n", __func__);
            delete_peer(p);
            epicsMutexUnlock(peerList.mutex);
            return NULL;
        }

        ellAdd(&peerList.list, &p->mpf.node);
    }
    epicsMutexUnlock(peerList.mutex);

    return p;
}

//
// Send request
//
long drvNetMpfSendRequest(TRANSACTION *t, bool atfront)
{
    LOGMSG("drvNetMpf: drvNetMpfSendRequest(%8p)\n", t);

    if (!t) {
        errlogPrintf("drvNetMpf: %s: null request\n", __func__);
        return ERROR;
    }

    PEER *p = (PEER *)t->facility;
    if (!p) {
        errlogPrintf("drvNetMpf: %s: null peer\n", __func__);
        return ERROR;
    }

    epicsMutexMustLock(p->reqQ_mutex);
    {
        if (! atfront) {
            //errlogPrintf("drvNetMpf: %s: ellAdd    :count=%d node=0x%p node.previous=0x%p\n", __func__, (p->reqQueue).count, &(p->reqQueue).node, (p->reqQueue).node.previous);
            ellAdd(&p->reqQueue, &t->node);
        } else {
            //errlogPrintf("drvNetMpf: %s: ellInsert :count=%d node=0x%p node.previous=0x%p\n", __func__, (p->reqQueue).count, &(p->reqQueue).node, (p->reqQueue).node.previous);
            ellInsert(&p->reqQueue, NULL, &t->node);
        }
    }
    epicsMutexUnlock(p->reqQ_mutex);

    // send signal to sent_task()
    epicsEventSignal(p->req_queued);

    return OK;
}

//
// Get request from queue
//
static TRANSACTION *get_request_from_queue(PEER *p)
{
    TRANSACTION *t;
    epicsMutexMustLock(p->reqQ_mutex);
    {
        t = (TRANSACTION *)ellGet(&p->reqQueue);
    }
    epicsMutexUnlock(p->reqQ_mutex);

    return t;
}

//
// Open a socket (and establish the connection, if TCP)
//
static void do_connect(PEER *p)
{
    MPF_COMMON *m = &(p->mpf);
    char errstr[512];
    LOGMSG("drvNetMpf: %s(%8p)\n", m, __func__);

    char *inet_string = inet_ntoa((struct in_addr)m->peer_addr.sin_addr);

    errno = 0;
    while ((m->sfd = socket(AF_INET,
                            isUdp(m->option) ?
                            SOCK_DGRAM : SOCK_STREAM,
                            0
                            )) == ERROR) {
        errlogPrintf("drvNetMpf: %s: socket() failed: %s[%d]\n", __func__, strerror_r(errno, errstr, sizeof(errstr)), errno);
        epicsThreadSleep(1.0);
    }

    if (isUdp(m->option)) {
        LOGMSG("drvNetMpf: protocol is UDP\n");

        struct sockaddr_in my_addr;

        memset(&my_addr, 0, sizeof(my_addr));
        my_addr.sin_family = AF_INET;
        my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        my_addr.sin_port = isOmron(m->option)? m->peer_addr.sin_port:htons(0);

        if (isOmron(m->option)) {
            errlogPrintf("drvNetMpf: %s: Omron was detected [port: %d]\n", __func__, ntohs(my_addr.sin_port));
        }

        while (bind(m->sfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) == ERROR) {
            errlogPrintf("drvNetMpf: %s: bind() failed: %s[%d]\n", __func__, strerror_r(errno, errstr, sizeof(errstr)), errno);
            epicsThreadSleep(1.0);
        }

        const int port = ntohs(m->peer_addr.sin_port);
        errlogPrintf("drvNetMpf: %s: socket opened for %s:0x%4x(%d)/%s\n", __func__, inet_string, port, port, getProtocolName(m->option));
    } else {
        // turn on KEEPALIVE so if the client system crashes
        // this task will find out and suspend
        int opt = TRUE;
        while (setsockopt(m->sfd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) == ERROR) {
            errlogPrintf("drvNetMpf: %s: setsockopt(SO_KEEPALIVE) failed: %s[%d]\n", __func__, strerror_r(errno, errstr, sizeof(errstr)), errno);
            epicsThreadSleep(1.0);
        }

        do {
            errlogPrintf("drvNetMpf: %s: tcp client trying to connect to %s ...\n", __func__, inet_string);
            epicsThreadSleep(1.0);
        } while (connect(m->sfd, (struct sockaddr *)&m->peer_addr, (socklen_t)sizeof(m->peer_addr)) == ERROR);

        const int port = ntohs(m->peer_addr.sin_port);
        errlogPrintf("drvNetMpf: %s: connected to %s:0x%4x(%d)/%s\n", __func__, inet_string, port, port, getProtocolName(m->option)); // in the case of UDP, this message is not quite accurate...

        //setReconn(m->option);
    }

    // set send timeout...
    if (p->send_timeout>0) {
        struct timeval timeout;
        timeout.tv_sec = p->send_timeout;
        timeout.tv_usec = (p->send_timeout - timeout.tv_sec) * 1e6;
        //DEBUG
        //errlogPrintf("drvNetMpf: send timeout : %ld.%06ld\n", timeout.tv_sec, timeout.tv_usec);

        while (setsockopt(m->sfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
            errlogPrintf("drvNetMpf: setsockopt(SO_SNDTIMEO) failed: %s\n", strerror_r(errno, errstr, sizeof(errstr)));
            epicsThreadSleep(1.0);
        }
    }

    // and recv timeout
    if (p->recv_timeout>0) {
        struct timeval timeout;
        timeout.tv_sec = p->recv_timeout;
        timeout.tv_usec = (p->recv_timeout - timeout.tv_sec) * 1e6;
        //DEBUG
        //errlogPrintf("drvNetMpf: recv timeout : %ld.%06ld\n", timeout.tv_sec, timeout.tv_usec);

        while (setsockopt(m->sfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            errlogPrintf("drvNetMpf: setsockopt(SO_RCVTIMEO) failed: %s\n", strerror_r(errno, errstr, sizeof(errstr)));
            epicsThreadSleep(1.0);
        }
    }
}

//
// Send message
//
static int send_msg(MPF_COMMON *m)
{
    char  errstr[512];
    void *cur = m->send_buf;
    errno = 0;

    if (isUdp(m->option)) {
        ssize_t len = sendto(m->sfd,
                             cur,
                             m->send_len,
                             0,
                             &m->peer_addr,
                             sizeof(m->peer_addr)
                             );
        if (len != m->send_len) {
            //if (errno == EAGAIN || errno == EWOULDBLOCK) { // timeout occured
            //    //... do we need a special treatment for timeout?
            //    return TIMEOUT;
            //}
            errlogPrintf("drvNetMpf: %s: sendto() failed: %s[%d]\n", __func__, strerror_r(errno, errstr, sizeof(errstr)), errno);
            return ERROR;
        }

        cur += len;
    } else {
        while (m->send_len > 0) {
            ssize_t len = send(m->sfd,
                               cur,
                               m->send_len,
                               0
                               );
            if (len == ERROR) {
                //if (errno == EAGAIN || errno == EWOULDBLOCK) { // timeout occured
                //    //... do we need a special treatment for timeout?
                //    return TIMEOUT;
                //}
                errlogPrintf("drvNetMpf: %s: send() failed: %s[%d]\n", __func__, strerror_r(errno, errstr, sizeof(errstr)), errno);
                return RECONNECT; // RECONNECT is not handled in send_task() ...
            }

            m->send_len -= len;
            cur += len;
        }
    }

    m->send_len = cur - m->send_buf;

    DUMP_MSG(m, m->send_buf, cur - m->send_buf, SEND);
    do_showmsg(m, m->send_buf, cur - m->send_buf, SEND);

    return OK;
}

//
// Receive message
//
static int recv_msg(MPF_COMMON *m)
{
    char errstr[512];
    void *cur = m->recv_buf;
    void *end = m->recv_buf + RECV_BUF_SIZE(m->option);
    errno = 0;

    memset(cur, 0, RECV_BUF_SIZE(m->option));

    if (isUdp(m->option)) {
        socklen_t sender_addr_len = sizeof(m->sender_addr);
        memset(&m->sender_addr, 0, sender_addr_len);

        if (m->recv_len > RECV_BUF_SIZE(m->option)) {
            errlogPrintf("drvNetMpf: %s: receive buffer running short (requested:%zd, max:%d)\n", __func__, m->recv_len, RECV_BUF_SIZE(m->option));
            return ERROR;
        }

        ssize_t len = recvfrom(m->sfd,
                               cur,
                               m->recv_len ? m->recv_len : RECV_BUF_SIZE(m->option),
                               0,
                               (struct sockaddr *)&m->sender_addr,
                               &sender_addr_len
                               );

        if (len == ERROR) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) { // timeout occured
                //DEBUG
                //errlogPrintf("drvNetMpf: %s: recv() timed out\n", __func__);
                return TIMEOUT;
            }

            errlogPrintf("drvNetMpf: %s: recvfrom() failed: %s[%d]\n", __func__, strerror_r(errno, errstr, sizeof(errstr)), errno);
            return ERROR;
        }

        cur += len;
    } else {
        do {
            if (m->recv_len > end - cur) {
                errlogPrintf("drvNetMpf: %s: receive buffer running short (requested:%zd, max:%zd)\n", __func__, m->recv_len, end - cur);
                return ERROR;
            }

            ssize_t len = recv(m->sfd,
                               cur,
                               m->recv_len ? m->recv_len : RECV_BUF_SIZE(m->option),
                               0
                               );

            if (len == ERROR) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) { // timeout occured
                    errlogPrintf("drvNetMpf: %s: recv() timed out\n", __func__);
                    return TIMEOUT;
                }
                errlogPrintf("drvNetMpf: %s: recv() failed: %s[%d]\n", __func__, strerror_r(errno, errstr, sizeof(errstr)), errno);
                return RECONNECT;
            } else if (len == 0) {
                errlogPrintf("drvNetMpf: %s: received EOF\n", __func__);
                return RECONNECT;
            }

            m->recv_len -= len;
            cur += len;
        } while (m->recv_len > 0);
    }

    m->recv_len = cur - m->recv_buf;

    DUMP_MSG(m, m->recv_buf, cur - m->recv_buf, RECV);
    do_showmsg(m, m->recv_buf, cur - m->recv_buf, RECV);

    return OK;
}

//
// Send task
//
static void send_task(PEER *p)
{
    //DEBUG
    //errlogPrintf("%s\n", __func__);

    for (;;) {
        // wait for signal from drvNetMpfSendRequest()
        epicsEventMustWait(p->req_queued);

        //
        TRANSACTION *t;
        while ((t = get_request_from_queue(p))) {
            //DEBUG
            //errlogPrintf("drvNetMpf: got request from %s\n", t->record->name);
            LOGMSG("drvNetMpf: got request from \"%s\"\n", t->record->name);

            if (isOmron(p->mpf.option)) {
                p->mpf.peer_addr = t->peer_addr;
            }

            t->transaction_id = p->mpf.counter++;
            memset(p->mpf.send_buf, 0, SEND_BUF_SIZE(p->mpf.option));
            p->mpf.send_len = SEND_BUF_SIZE(p->mpf.option);

            int ret = t->config_command(t->record,
                                        &t->option,
                                        p->mpf.send_buf,
                                        &p->mpf.send_len,
                                        t->device,
                                        t->transaction_id
                                        );
            if (ret == ERROR) {
                t->ret = ret;
                callbackRequest(&t->io.async.callback);
                continue;
            }

            if (ret >= 0) {
                p->mpf.recv_len = ret;
            }

            if (!t->parse_response || ret == NO_RESP) {
                do_showio(t, 1);
                t->ret = send_msg(&p->mpf);

                //
                if (!t->ret && ret == NOT_DONE) {
                    drvNetMpfSendRequest(t, isAtFront(t->option));
                } else {
                    callbackRequest(&t->io.async.callback);
                }

                continue;
            }
#ifdef SANITY_CHECK
            {
                // sanity check
                uint32_t delta = - t->io.async.send_counter
                                 + t->io.async.senderr_counter
                                 + t->io.async.receive_counter
                                 + t->io.async.timeout_counter
                                 + t->io.async.recverr_counter
                                 ;

                if (delta) {
                    errlogPrintf("drvNetMpf: %s: doubled callback detected (delta:%d) s=%d s_err=%d r=%d t=%d r_err=%d\n", __func__, delta,
                                 t->io.async.send_counter, t->io.async.senderr_counter, t->io.async.receive_counter, t->io.async.timeout_counter, t->io.async.recverr_counter);
                }
#ifdef SANITY_PRINT
                else if (!(t->io.async.send_counter % 10000)) {
                    static int i = 0;

                    errlogPrintf("drvNetMpf: sanity check OK (%d times)\n", i++);
                    errlogPrintf("send    : %d times\n", t->io.async.send_counter);
                    errlogPrintf("senderr : %d times\n", t->io.async.senderr_counter);
                    errlogPrintf("receive : %d times\n", t->io.async.receive_counter);
                    errlogPrintf("timeout : %d times\n", t->io.async.timeout_counter);
                    errlogPrintf("recverr : %d times\n", t->io.async.recverr_counter);
                    errlogPrintf("delta   : %d times\n", delta);
                }
#endif // SANITY_PRINT
            }
#endif // SANITY_CHECK

            t->io.async.timeout_flag = 0;
            p->in_transaction = t;
            t->io.async.send_counter++;
            if (p->epicsTimer_timeout>0) {
                // We're using epicsTimer
                epicsTimerStartDelay(p->wd_id, p->epicsTimer_timeout);
            }

            //
            do_showio(t, 1);
            t->ret = send_msg(&p->mpf);

            //
            if (t->ret) {
                epicsTimerCancel(p->wd_id);
                epicsMutexMustLock(p->in_t_mutex);
                {
                    t = p->in_transaction;
                    p->in_transaction = 0;
                }
                epicsMutexUnlock(p->in_t_mutex);

                if (t) {
                    callbackRequest(&t->io.async.callback);
                    t->io.async.senderr_counter++;
                }

                continue;
            }

            epicsTimeGetCurrent(&p->send_time);

            //DEBUG
            //errlogPrintf("drvNetMpf: %s: %s                                     (%9d.%09u)\n", __func__, t->record->name, p->send_time.secPastEpoch, p->send_time.nsec);

            if (p->send_timeout>0 || p->recv_timeout>0) {
                // send signal to recv_task()
                epicsEventSignal(p->next_rsp);
            }

            // wait for signal from recv_task()
            epicsEventMustWait(p->next_cmd);
        }
    }
}

//
// Receve task
//
static void recv_task(PEER *p)
{
    //DEBUG
    //errlogPrintf("%s\n", __func__);

    for (;;) {
        if (p->send_timeout>0 || p->recv_timeout>0) {
            // wait signal from send_task()
            epicsEventMustWait(p->next_rsp);
        }

        const int ret = recv_msg(&p->mpf);
        epicsTimerCancel(p->wd_id);
        epicsTimeGetCurrent(&p->recv_time);

        if (ret == RECONNECT) {
            // close the socket ...
            shutdown(p->mpf.sfd, SHUT_RDWR);
            close(p->mpf.sfd);
            p->mpf.sfd = 0; // just in case

            // ... and open it again
            epicsThreadSleep(1.0);
            do_connect(p);

            continue; // is this OK? don't we have to goto finish_process, for example?
        }

        TRANSACTION *t;
        epicsMutexMustLock(p->in_t_mutex);
        {
            t = p->in_transaction;
            p->in_transaction = 0;
        }
        epicsMutexUnlock(p->in_t_mutex);

        const double rtt = (p->recv_time.secPastEpoch - p->send_time.secPastEpoch) * 1e3
                           + (p->recv_time.nsec - p->send_time.nsec) * 1e-6;

        if (!t) {
            errlogPrintf("drvNetMpf: %s: discarding stray response: rtt=%.3f ms\n", __func__, rtt);
            //errlogPrintf("drvNetMpf: %s: discarding stray response: rtt=%.3f ms (%9d.%09u - %9d.%09u)\n", __func__, rtt, p->recv_time.secPastEpoch, p->recv_time.nsec, p->send_time.secPastEpoch, p->send_time.nsec);
            goto recv_next_rsp;
        }

        //
        t->ret = 0;

        if (ret == TIMEOUT) { // Timeout occurred in recv_msg()
            // save send_time, which will be used for RTT of timed out response
            p->send_time_timeout.secPastEpoch = p->send_time.secPastEpoch;
            p->send_time_timeout.nsec         = p->send_time.nsec;

            //
            if (0 && p->timeout_state) {
                errlogPrintf("drvNetMpf: %s: %s timed out during recovery: rtt=%.3f ms\n", __func__, t->record->name, rtt);
                //errlogPrintf("drvNetMpf: %s: %s timed out during recovery: rtt=%.3f ms (%9d.%09u - %9d.%09u)\n", __func__, t->record->name, rtt, p->recv_time.secPastEpoch, p->recv_time.nsec, p->send_time.secPastEpoch, p->send_time.nsec);

                // timed out during recovery process from previous timeout.
                p->timeout_state = false;
                goto send_next_cmd;
            }

            errlogPrintf("drvNetMpf: %s: %s timed out: rtt=%.3f ms)\n", __func__, t->record->name, rtt);
            //errlogPrintf("drvNetMpf: %s: %s timed out: rtt=%.3f ms (%9d.%09u - %9d.%09u)\n", __func__, t->record->name, rtt, p->recv_time.secPastEpoch, p->recv_time.nsec, p->send_time.secPastEpoch, p->send_time.nsec);

            t->io.async.timeout_counter++;
            p->timeout_state = true;
            p->timeout_count++;

            // t->ret must be set as it is used by netDevReadWriteXx()
            t->ret = TIMEOUT;
            goto finish_process;
        }

        if (ret != OK) { // Some other error occurred in recv_msg()
            errlogPrintf("drvNetMpf: %s: %s receive error: rtt=%.3f ms\n", __func__, t->record->name, rtt);
            //errlogPrintf("drvNetMpf: %s: %s receive error: rtt=%.3f ms (%9d.%09u - %9d.%09u)\n", __func__, t->record->name, rtt, p->recv_time.secPastEpoch, p->recv_time.nsec, p->send_time.secPastEpoch, p->send_time.nsec);

            t->io.async.recverr_counter++;

            // t->ret must be set as it is used by netDevReadWriteXx()
            t->ret = ERROR;
            goto finish_process;
        }

        // Successfully received data via recv_msg(), now parse it.
        t->ret = t->parse_response(t->record,
                                   &t->option,
                                   p->mpf.recv_buf,
                                   &p->mpf.recv_len,
                                   t->device,
                                   t->transaction_id
                                   );

        if (t->ret == NOT_MINE) {
            p->in_transaction = t;

            if (p->timeout_state) {
                const double rtt2 = (p->recv_time.secPastEpoch - p->send_time_timeout.secPastEpoch) * 1e3
                                   + (p->recv_time.nsec - p->send_time_timeout.nsec) * 1e-6;
                errlogPrintf("drvNetMpf: %s: %s Discarding timed out response: rtt=%.3f ms\n", __func__, t->record->name, rtt2);
                //errlogPrintf("drvNetMpf: %s: %s Discarding timed out response: rtt=%.3f ms (%9d.%09u - %9d.%09u)\n", __func__, t->record->name, rtt2, p->recv_time.secPastEpoch, p->recv_time.nsec, p->send_time_timeout.secPastEpoch, p->send_time_timeout.nsec);
                if (rtt2>5000) {
                    abort();
                }
            } else {
                errlogPrintf("drvNetMpf: %s: %s Discarding unexpected response: rtt=%.3f ms\n", __func__, t->record->name, rtt);
                //errlogPrintf("drvNetMpf: %s: %s Discarding unexpected response: rtt=%.3f ms (%9d.%09u - %9d.%09u)\n", __func__, t->record->name, rtt, p->recv_time.secPastEpoch, p->recv_time.nsec, p->send_time.secPastEpoch, p->send_time.nsec);
            }

            if (p->epicsTimer_timeout>0) {
                // use epics timer
                epicsTimerStartDelay(p->wd_id, p->epicsTimer_timeout);
            }

            if (p->send_timeout>0 || p->recv_timeout>0) {
                // send signal to myself
                epicsEventSignal(p->next_rsp);
            }

            goto recv_next_rsp;
        }

        // The received data was the expected one
        t->io.async.receive_counter++;

        if (t->ret == RECOVERED) {
            //DEBUG
            //errlogPrintf("drvNetMpf: %s: %s recovered from timeout: rtt=%.3f ms (%9d.%09u - %9d.%09u)\n", __func__, t->record->name, rtt, p->recv_time.secPastEpoch, p->recv_time.nsec, p->send_time.secPastEpoch, p->send_time.nsec);

            p->timeout_state = false;
            p->send_time_timeout.secPastEpoch = 0;
            p->send_time_timeout.nsec         = 0;
            p->in_transaction = t;

            drvNetMpfSendRequest(t, true); // insert at front

            goto send_next_cmd;
        }

        //DEBUG
        //errlogPrintf("drvNetMpf: %s: %s rtt=%.3f ms (%9d.%09u - %9d.%09u)\n", __func__, t->record->name, rtt, p->recv_time.secPastEpoch, p->recv_time.nsec, p->send_time.secPastEpoch, p->send_time.nsec);

        do_showio(t, 0);

        if (t->ret == RECV_MORE) {
            // RECV_MORE is no used at all
            p->in_transaction = t;

            if (p->epicsTimer_timeout>0) {
                // use epicsTimer
                epicsTimerStartDelay(p->wd_id, p->epicsTimer_timeout);
            }

            if (p->send_timeout>0 || p->recv_timeout>0) {
                // send signal to myself
                epicsEventSignal(p->next_rsp);
            }

            continue;
        }

        if (t->ret == NOT_DONE) {
            drvNetMpfSendRequest(t, isAtFront(t->option));
            goto send_next_cmd;
        }

        if (t->ret < 0) {
            // This is unlikely to happen but a bug in the device support
            errlogPrintf("%s : drvNetMpf: %s: \"parse_response()\" callback returned unknown error code %d\n", t->record->name, __func__, t->ret);
        }

    finish_process:
        // Successfully received all data (or read error or timeout ...)
        LOGMSG("drvNetMpf: requesting callback for \"%s\"\n", t->record->name);
        callbackRequest(&t->io.async.callback);

    send_next_cmd:
        do_showrtt(p);

        // send signal to send_task()
        epicsEventSignal(p->next_cmd);

    recv_next_rsp:
        p->mpf.recv_len = 0;

    }
}

//
// Timeout handler
//
static void mpf_timeout_handler(PEER *p)
{
    TRANSACTION *t;
    epicsMutexMustLock(p->in_t_mutex);
    {
        t = p->in_transaction;
        p->in_transaction = 0;
    }
    epicsMutexUnlock(p->in_t_mutex);

    if (t) {
        //epicsTimeStamp current;
        //char time[256];
        //epicsTimeGetCurrent(&current);
        //epicsTimeToStrftime(time, sizeof(time), "%Y-%m-%d %H:%M:%S.%06f", &current);
        //errlogPrintf("drvNetMpf: *** mpf_timeout_handler(\"%s\"):%s ***\n", t->record->name, time);
        errlogPrintf("drvNetMpf: *** mpf_timeout_handler(\"%s\") ***\n", t->record->name);

        t->io.async.timeout_flag = 1;
        t->io.async.timeout_counter++;
        callbackRequest(&t->io.async.callback);
        epicsEventSignal(p->next_cmd);

        if (p->tmo_event) {
            post_event(p->event_num);

            errlogPrintf("drvNetMpf: ###### tmo event(%d) issued ######\n", p->event_num);
        }
    }
}

//
// Register event
//
long drvNetMpfRegisterEvent(TRANSACTION *t)
{
    LOGMSG("drvNetMpf: drvNetMpfRegisterEvent(%8p)\n", t);

    if (!t) {
        errlogPrintf("drvNetMpf: %s: null event\n", __func__);
        return ERROR;
    }

    SERVER *s;
    if (!(s = (SERVER *)t->facility)) {
        errlogPrintf("drvNetMpf: %s: null server\n", __func__);
        return ERROR;
    }

    epicsMutexMustLock(s->eventQ_mutex);
    {
        ellAdd(&s->eventQueue, &t->node);
    }
    epicsMutexUnlock(s->eventQ_mutex);

    return OK;
}

static int tcp_parent(SERVER *s)
{
    char errstr[512];
    if ((s->mpf.sfd = socket(AF_INET,
                             SOCK_STREAM,
                             0
                             )) == ERROR) {
        errlogPrintf("drvNetMpf: %s: socket() failed: %s[%d]\n", __func__, strerror_r(errno, errstr, sizeof(errstr)), errno);
        return ERROR;
    }

    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family      = AF_INET;
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr.sin_port        = htons(s->port);

    for (;;) {
        errlogPrintf("drvNetMpf: %s: tcp parent: trying to bind...\n", __func__);

        if (bind(s->mpf.sfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) == OK) {
            break;
        }

        epicsThreadSleep(10.0);
    }

    if (listen(s->mpf.sfd, 10) == ERROR) {
        errlogPrintf("drvNetMpf: %s: listen() failed: %s[%d]\n", __func__, strerror_r(errno, errstr, sizeof(errstr)), errno);
        close(s->mpf.sfd);
        return ERROR;
    }

    while (TRUE) {
        socklen_t sender_addr_len = sizeof(s->mpf.sender_addr);
        memset(&s->mpf.sender_addr, 0, sender_addr_len);

        int new_sfd = accept(s->mpf.sfd, (struct sockaddr *)&s->mpf.sender_addr, &sender_addr_len);
        if (new_sfd == ERROR) {
            errlogPrintf("drvNetMpf: %s: accept() failed: %s[%d]\n", __func__, strerror_r(errno, errstr, sizeof(errstr)), errno);
            epicsThreadSleep(15.0);
            continue;
        }

        // turn on KEEPALIVE so if the client system crashes
        // this task will find out and suspend
        int opt = TRUE;
        if (setsockopt(new_sfd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)
                       ) == ERROR) {
            errlogPrintf("drvNetMpf: %s: setsockopt(SO_KEEPALIVE) failed: %s[%d]\n", __func__, strerror_r(errno, errstr, sizeof(errstr)), errno);
            return ERROR;
        }

        TRANSACTION *t;
        epicsMutexMustLock(s->eventQ_mutex);
        {
            for (t = (TRANSACTION *)ellFirst(&s->eventQueue); t; t = (TRANSACTION *)ellNext(&t->node)) {
                if (t->io.event.client_addr.sin_addr.s_addr
                    == s->mpf.sender_addr.sin_addr.s_addr) {
                    ellDelete(&s->eventQueue, &t->node);
                    break;
                }
            }
        }
        epicsMutexUnlock(s->eventQ_mutex);

        char *visitor = inet_ntoa((struct in_addr)s->mpf.sender_addr.sin_addr);

        if (!t) {
            errlogPrintf("drvNetMpf: %s: unexpected connection request from %s\n", __func__, visitor);
            close(new_sfd);
            continue; // while (TRUE)
        }

        errlogPrintf("drvNetMpf: %s: connection request from %s\n", __func__, visitor);

        SERVER *c = calloc(1, sizeof(SERVER));
        if (!c) {
            errlogPrintf("drvNetMpf: %s: calloc failed\n", __func__);
            close(new_sfd);
            continue; // while (TRUE)
        }

        c->mpf.sfd = new_sfd;
        c->mpf.id = globalServerCount++;
        c->mpf.option = s->mpf.option;
        c->parent = s;
        ellInit(&c->eventQueue);

        if ((c->eventQ_mutex = epicsMutexCreate()) == 0) {
            errlogPrintf("drvNetMpf: %s: epicsMutexCreate failed\n", __func__);
            delete_server(c);
            continue; // while (TRUE)
        }

        epicsMutexMustLock(c->eventQ_mutex);
        {
            ellAdd(&c->eventQueue, &t->node);
        }
        epicsMutexUnlock(c->eventQ_mutex);

        c->mpf.send_buf = calloc(1, SEND_BUF_SIZE(c->mpf.option));
        if (!c->mpf.send_buf) {
            errlogPrintf("drvNetMpf: %s: calloc failed (send_buf)\n", __func__);
            delete_server(c);
            continue; // while (TRUE)
        }

        c->mpf.recv_buf = calloc(1, RECV_BUF_SIZE(c->mpf.option));
        if (!c->mpf.recv_buf) {
            errlogPrintf("drvNetMpf: %s: calloc failed (recv_buf)\n", __func__);
            delete_server(c);
            continue; // while (TRUE)
        }

        if (spawn_server_task(c) == ERROR) {
            errlogPrintf("drvNetMpf: %s: spawn_sever_tasks failed\n", __func__);
            delete_server(c);
            continue; // while (TRUE)
        }

        epicsMutexMustLock(serverList.mutex);
        {
            ellAdd(&serverList.list, &c->mpf.node);
        }
        epicsMutexUnlock(serverList.mutex);
    }
}

//
// Create TCP parent
//
static long spawn_tcp_parent(SERVER *s)
{
    LOGMSG("drvNetMpf: spawn_tcp_parent(%8p)\n", s);

    char *parent_t_name = "tTcpSrvr";
    char  task_name[32];
    sprintf(task_name, "%s", parent_t_name);

    if (epicsThreadCreate(task_name,
                          TCP_PARENT_PRIORITY,
                          TCP_PARENT_STACK,
                          (EPICSTHREADFUNC)tcp_parent,
                          s
                          ) == NULL) {
        errlogPrintf("drvNetMpf: %s: epicsThreadCreate failed\n", __func__);
        return ERROR;
    }

    return OK;
}

//
// Create socket and connect
//
static long prepare_udp_server_socket(SERVER *s)
{
    char errstr[512];
    LOGMSG("drvNetMpf: prepare_udp_server_socket(%8p)\n", s);

    if ((s->mpf.sfd = socket(AF_INET,
                             SOCK_DGRAM,
                             0
                             )) == ERROR) {
        errlogPrintf("drvNetMpf: %s: socket() failed %s[%d]\n", __func__, strerror_r(errno, errstr, sizeof(errstr)), errno);
        return ERROR;
    }

    LOGMSG("drvNetMpf: protocol is UDP\n");

    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr.sin_port = htons(s->port);

    if (bind(s->mpf.sfd,
             (struct sockaddr *)&my_addr,
             (int)sizeof(my_addr)
             ) == ERROR) {
        errlogPrintf("drvNetMpf: %s: bind() failed %s[%d]\n", __func__, strerror_r(errno, errstr, sizeof(errstr)), errno);
        return ERROR;
    }

    return OK;
}

//
// Spawn server task
//
static long spawn_server_task(SERVER *s)
{
    LOGMSG("drvNetMpf: spawn_server_task(%8p)\n", s);

    char *send_t_name = "tEvSrvr";
    char  task_name[32];
    sprintf(task_name, "%s_%d", send_t_name, s->mpf.id);

    if ((s->server_tid = epicsThreadCreate(task_name,
                                           EVSRVR_PRIORITY,
                                           EVSRVR_STACK,
                                           (EPICSTHREADFUNC)event_server,
                                           s
                                           )) == 0) {
        s->server_tid = 0;
        errlogPrintf("drvNetMpf: %s: epicsThreadCreate failed\n", __func__);
        return ERROR;
    }

    return OK;
}

//
// Delete server
//
static void delete_server(SERVER *s)
{
    // This function should be called only before
    // the server is added to the serverList.
    LOGMSG("drvNetMpf: delete_server(%8p)\n", s);

    if (s->parent) {
        SERVER *parent = s->parent;
        TRANSACTION *t;
        epicsMutexMustLock(s->eventQ_mutex);
        {
            t = (TRANSACTION *)ellGet(&s->eventQueue);
        }
        epicsMutexUnlock(s->eventQ_mutex);

        epicsMutexMustLock(parent->eventQ_mutex);
        {
            ellAdd(&parent->eventQueue, &t->node);
        }
        epicsMutexUnlock(parent->eventQ_mutex);
    }

    if (s->mpf.sfd) {
        close(s->mpf.sfd);
    }
    if (s->eventQ_mutex) {
        epicsMutexDestroy(s->eventQ_mutex);
    }
    if (s->mpf.send_buf) {
        free(s->mpf.send_buf);
    }
    if (s->mpf.recv_buf) {
        free(s->mpf.recv_buf);
    }

    free(s);
}

//
// Create and initialize server structure
//
SERVER *drvNetMpfInitServer(unsigned short port, uint32_t option)
{
    LOGMSG("drvNetMpf: drvNetMpfInitServer(0x%04x,0x%08x)\n", port, option);

    SERVER *s;
    epicsMutexMustLock(serverList.mutex);
    {
        for (s = (SERVER *)ellFirst(&serverList.list); s; s = (SERVER *)ellNext(&s->mpf.node)) {
            if (s->port == port) {
                break;
            }
        }

        if (s) {
            if (GET_MPF_OPTION(s->mpf.option) != GET_MPF_OPTION(option)) {
                errlogPrintf("drvNetMpf: %s: server option dose not match\n", __func__);
                epicsMutexUnlock(serverList.mutex);
                return NULL;
            }

            // only one (parent) server for a specific port
            epicsMutexUnlock(serverList.mutex);
            return s;
        }

        s = calloc(1, sizeof(SERVER));
        if (!s) {
            errlogPrintf("drvNetMpf: %s: calloc failed\n", __func__);
            epicsMutexUnlock(serverList.mutex);
            return NULL;
        }

        s->port = port;
        s->mpf.id = globalServerCount++;
        s->mpf.option = GET_MPF_OPTION(option);
        ellInit(&s->eventQueue);

        if ((s->eventQ_mutex=epicsMutexCreate()) == 0) {
            errlogPrintf("drvNetMpf: %s: epicsMutexCreate failed\n", __func__);
            delete_server(s);
            epicsMutexUnlock(serverList.mutex);
            return NULL;
        }

        if (isTcp(option)) {
            if (spawn_tcp_parent(s)) {
                errlogPrintf("drvNetMpf: %s: spawn_tcp_parent failed\n", __func__);
                delete_server(s);
                epicsMutexUnlock(serverList.mutex);
                return NULL;
            }
        } else {
            if (prepare_udp_server_socket(s) == ERROR) {
                errlogPrintf("drvNetMpf: %s: prepare_udp_server_socket failed\n", __func__);
                delete_server(s);
                epicsMutexUnlock(serverList.mutex);
                return NULL;
            }

            s->mpf.send_buf = calloc(1, SEND_BUF_SIZE(s->mpf.option));
            if (!s->mpf.send_buf) {
                errlogPrintf("drvNetMpf: %s: calloc failed (send_buf)\n", __func__);
                delete_server(s);
                epicsMutexUnlock(serverList.mutex);
                return NULL;
            }

            s->mpf.recv_buf = calloc(1, RECV_BUF_SIZE(s->mpf.option));
            if (!s->mpf.recv_buf) {
                errlogPrintf("drvNetMpf: %s: calloc failed (recv_buf)\n", __func__);
                delete_server(s);
                epicsMutexUnlock(serverList.mutex);
                return NULL;
            }

            if (spawn_server_task(s) == ERROR) {
                errlogPrintf("drvNetMpf: %s: spawn_server_task failed\n", __func__);
                delete_server(s);
                epicsMutexUnlock(serverList.mutex);
                return NULL;
            }
        }

        ellAdd(&serverList.list, &s->mpf.node);
    }
    epicsMutexUnlock(serverList.mutex);

    return s;
}

//
// Searches for event acceptor
//
static int event_server(SERVER *s)
{
    for (;;) {
        if (!event_server_start_flag) {
            errlogPrintf("drvNetMpf: %s: event server %d waiting for getting started...\n", __func__, s->mpf.id);
            epicsThreadSleep(1.0);
            continue;
        }

        TRANSACTION *t;
        if (isTcp(s->mpf.option)) {
            t = (TRANSACTION *)ellFirst(&s->eventQueue);

            if (t && t->io.event.ev_msg_len) {
                s->mpf.recv_len = t->io.event.ev_msg_len;
            }
        }

        if (recv_msg(&s->mpf) == RECONNECT) {
            shutdown(s->mpf.sfd, SHUT_RDWR);
            close(s->mpf.sfd);
            errlogPrintf("drvNetMpf: %s: event server %d exitting...\n", __func__, s->mpf.id);
            break;
        }

        epicsMutexMustLock(s->eventQ_mutex);
        {
            for (t = (TRANSACTION *)ellFirst(&s->eventQueue); t; t = (TRANSACTION *)ellNext(&t->node)) {
                if (isUdp(s->mpf.option) &&
                    t->io.event.client_addr.sin_addr.s_addr != s->mpf.sender_addr.sin_addr.s_addr) {
                    continue;
                }

                if (t && t->record->scan == SCAN_IO_EVENT) {
                    t->ret = t->parse_response(t->record,
                                               &t->option,
                                               s->mpf.recv_buf,
                                               &s->mpf.recv_len,
                                               t->device,
                                               t->transaction_id
                                               );
                    if (t->ret != NOT_MINE) {
                        do_showio(t, 0);
                        LOGMSG("drvNetMpf: event_server working for \"%s\"\n", t->record->name);
                        break;
                    }
                }
            }
        }
        epicsMutexUnlock(s->eventQ_mutex);

        if (t && t->config_command && t->ret == OK) {
            t->transaction_id = s->mpf.counter++;
            memset(s->mpf.send_buf, 0, SEND_BUF_SIZE(s->mpf.option));
            s->mpf.send_len = SEND_BUF_SIZE(s->mpf.option);
            t->config_command(t->record,
                              &t->option,
                              s->mpf.send_buf,
                              &s->mpf.send_len,
                              t->device,
                              t->transaction_id
                              );
            s->mpf.peer_addr = s->mpf.sender_addr;
            do_showio(t, 1);
            t->ret = send_msg(&s->mpf);
        }

        if (t && t->io.event.ioscanpvt && t->ret != NOT_DONE) {
            scanIoRequest(t->io.event.ioscanpvt);
        }

        s->mpf.recv_len = 0;
    }

    remove_server(s);

    return OK;
}

//
// Remove server
//
static void remove_server(SERVER *target)
{
    SERVER *s;
    epicsMutexMustLock(serverList.mutex);
    {
        for (s = (SERVER *)ellFirst(&serverList.list); s; s = (SERVER *)ellNext(&s->mpf.node)) {
            if (s == target) {
                ellDelete(&serverList.list, &s->mpf.node);
                errlogPrintf("server: %s: %d removed\n", __func__, s->mpf.id);
                break;
            }
        }
    }
    epicsMutexUnlock(serverList.mutex);

    if (s) {
        delete_server(s);
    }
}

//
// Starts event server
//
void startEventServer(const iocshArgBuf *args)
{
    event_server_start_flag = 1;
}

//
// Prints diagnosis facilities
//
void mpfHelp(const iocshArgBuf *args)
{
    printf("\n");
    printf(" -----------------------------------------------------------------\n");
    printf(" You have the four diagnostic routines listed and explained below.\n");
    printf(" -----------------------------------------------------------------\n");
    printf("\n");
    printf(" ----------\n");
    printf(" peerShow()\n");
    printf(" ----------\n");
    printf(" SYNOPSIS: int peerShow(\n");
    printf("                        int *peerId, /* peer ID */\n");
    printf("                        int level    /* interest level, 0, 1 or 2 */\n");
    printf("                       )\n");
    printf(" DISCRIPTION: display information in a peer structure\n");
    printf("\n");
    printf(" -------------\n");
    printf(" peerShowAll()\n");
    printf(" -------------\n");
    printf(" SYNOPSIS: int peerShowAll(\n");
    printf("                           int level /* interest level, 0, 1 or 2 */\n");
    printf("                          )\n");
    printf(" DISCRIPTION: iterate peerShow() for all of the peer sturucures\n");
    printf("\n");
    printf("\n");
    printf(" ------------\n");
    printf(" serverShow()\n");
    printf(" ------------\n");
    printf(" SYNOPSIS: int serverShow(\n");
    printf("                          int *serverId, /* server ID */\n");
    printf("                          int level    /* interest level, 0, 1 or 2 */\n");
    printf("                         )\n");
    printf(" DISCRIPTION: display information in a server structure\n");
    printf("\n");
    printf(" ---------------\n");
    printf(" serverShowAll()\n");
    printf(" ---------------\n");
    printf(" SYNOPSIS: int serverShowAll(\n");
    printf("                             int level /* interest level, 0, 1 or 2 */\n");
    printf("                            )\n");
    printf(" DISCRIPTION: iterate serverShow() for all of the server sturucures\n");
    printf("\n");
}

//
// Show peer info
//
void peerShow(const iocshArgBuf *args)
{
    PEER *p;
    epicsMutexMustLock(peerList.mutex);
    {
        for (p = (PEER *)ellFirst(&peerList.list); p; p = (PEER *)ellNext(&p->mpf.node)) {
            if (p->mpf.id == args[0].ival) {
                break;
            }
        }
    }
    epicsMutexUnlock(peerList.mutex);

    if (p) {
        char *inet_string = inet_ntoa(p->mpf.peer_addr.sin_addr);
        printf("  ------------------------------\n");
        printf("  id:                %d\n", p->mpf.id);
        printf("  protocol:          %s\n", getProtocolName(p->mpf.option));
        printf("  socket_fd:         %d\n", p->mpf.sfd);
        printf("  addrss_family:     0x%04x\n", p->mpf.peer_addr.sin_family);
        printf("  intenet_addr:      %s\n", inet_string);
        printf("  port_number:       0x%04x\n", ntohs(p->mpf.peer_addr.sin_port));
        printf("  in_transaction:    %8p\n", p->in_transaction);
        printf("  in_t_mutex_sem_id: %8p\n", p->in_t_mutex);
        printf("  num_requests:      %d\n", ellCount(&p->reqQueue));
        printf("  reqQ_mutex_sem_id: %8p\n", p->reqQ_mutex);
        printf("  req_queued_sem_id: %8p\n", p->req_queued);
        printf("  next_cmd_sem_id: %8p\n", p->next_cmd);
        printf("  next_rsp_sem_id: %8p\n", p->next_rsp);
        printf("  watchdog_id:       %8p\n", p->wd_id);
        printf("  send_task_id:      %8p\n", p->send_tid);
        printf("  recv_task_id:      %8p\n", p->recv_tid);
        printf("  send_msg_len:      %zd\n", p->mpf.send_len);
        printf("  recv_msg_len:      %zd\n", p->mpf.recv_len);
        printf("  send_buf_addr:     %8p\n", p->mpf.send_buf);
        printf("  recv_buf_addr:     %8p\n", p->mpf.recv_buf);
    }

    //if (args[1].ival > 1) {
    //    d(p->mpf.recv_buf, 0, 1);
    //    printf("\n\n\n");
    //}
}

//
// peerShow for all peers
//
void peerShowAll(const iocshArgBuf *args)
{
    PEER *p;
    epicsMutexMustLock(peerList.mutex);
    {
        for (p = (PEER *)ellFirst(&peerList.list); p; p = (PEER *)ellNext(&p->mpf.node)) {
            char *inet_string = inet_ntoa((struct in_addr)p->mpf.peer_addr.sin_addr);
            char *protocol = getProtocolName(p->mpf.option);
            uint16_t port = ntohs(p->mpf.peer_addr.sin_port);

            printf("Peer %d: %s %d(0x%x)/%s\n", p->mpf.id, inet_string, port, port, protocol);
        }
    }
    epicsMutexUnlock(peerList.mutex);
}

//
// Show server info
//
void serverShow(const iocshArgBuf *args)
{
    SERVER *s;
    epicsMutexMustLock(serverList.mutex);
    {
        for (s = (SERVER *)ellFirst(&serverList.list); s; s = (SERVER *)ellNext(&s->mpf.node)) {
            if (s->mpf.id == args[0].ival) {
                break;
            }
        }
    }
    epicsMutexUnlock(serverList.mutex);

    if (s) {
        char *inet_string = inet_ntoa(s->mpf.sender_addr.sin_addr);
        printf("  ------------------------------\n");
        printf("  id:                  %d\n", s->mpf.id);
        printf("  protocol:            %s\n", getProtocolName(s->mpf.option));
        printf("  socket_fd:           %d\n", s->mpf.sfd);
        printf("  port:                0x%04x\n", s->port);
        printf("  last_client(family): 0x%04x\n", s->mpf.sender_addr.sin_family);
        printf("  last_client(inaddr): %s\n", inet_string);
        printf("  last_client(port):   0x%04x\n", ntohs(s->mpf.sender_addr.sin_port));
        printf("  num_event_acceptor:  %d\n", ellCount(&s->eventQueue));
        printf("  evQ_mutex_sem_id:    %8p\n", s->eventQ_mutex);
        printf("  server_task_id:      %8p\n", s->server_tid);
        printf("  send_msg_len:        %zd\n", s->mpf.send_len);
        printf("  recv_msg_len:        %zd\n", s->mpf.recv_len);
        printf("  send_buf:            %8p\n", s->mpf.send_buf);
        printf("  recv_buff_addr:      %8p\n", s->mpf.recv_buf);
    }

    //if (args[1].ival > 1) {
    //    d(s->mpf.recv_buf, 0, 1);
    //    printf("\n\n\n");
    //}
}

//
// serverShow for all servers
//
void serverShowAll(const iocshArgBuf *args)
{
    SERVER *s;

    epicsMutexMustLock(serverList.mutex);
    {
        for (s = (SERVER *)ellFirst(&serverList.list); s; s = (SERVER *)ellNext(&s->mpf.node)) {
            char *inet_string = inet_ntoa((struct in_addr)s->mpf.sender_addr.sin_addr);
            char *protocol = getProtocolName(s->mpf.option);
            printf("Server %d: %s %d(0x%x)/%s\n",
                   s->mpf.id, inet_string, s->port, s->port, protocol);
        }
    }
    epicsMutexUnlock(serverList.mutex);
}

//
//
//
void showMsg(const iocshArgBuf *args)
{
    PEER *p;
    epicsMutexMustLock(peerList.mutex);
    {
        for (p = (PEER *)ellFirst(&peerList.list); p; p = (PEER *)ellNext(&p->mpf.node)) {
            if (p->mpf.id == args[0].ival) {
                p->mpf.show_msg = args[1].ival? 2:1;
                break;
            }
        }
    }
    epicsMutexUnlock(peerList.mutex);

    if (!p) {
        printf("peer %d not found\n", args[0].ival);
    }
}

//
//
//
void stopMsg(const iocshArgBuf *args)
{
    PEER *p;
    epicsMutexMustLock(peerList.mutex);
    {
        for (p = (PEER *)ellFirst(&peerList.list); p; p = (PEER *)ellNext(&p->mpf.node)) {
            p->mpf.show_msg = 0;
        }
    }
    epicsMutexUnlock(peerList.mutex);
}

//
//
//
void showEventMsg(const iocshArgBuf *args)
{
    SERVER *s;
    epicsMutexMustLock(serverList.mutex);
    {
        for (s = (SERVER *)ellFirst(&serverList.list); s; s = (SERVER *)ellNext(&s->mpf.node)) {
            if (s->mpf.id == args[0].ival) {
                s->mpf.show_msg = args[1].ival? 2:1;
                break;
            }
        }
    }
    epicsMutexUnlock(serverList.mutex);

    if (!s) {
        printf("server %d not found\n", args[0].ival);
    }
}

//
//
//
void stopEventMsg(const iocshArgBuf *args)
{
    SERVER *s;
    epicsMutexMustLock(serverList.mutex);
    {
        for (s = (SERVER *)ellFirst(&serverList.list); s; s = (SERVER *)ellNext(&s->mpf.node)) {
            s->mpf.show_msg = 0;
        }
    }
    epicsMutexUnlock(serverList.mutex);
}

//
// Dump message
//
void dump_msg(uint8_t *pbuf, ssize_t count, DIRECTION direction, bool is_binary)
{
    if (direction==SEND) {
        printf("=>");
    } else {
        printf("<=");
    }

    while (count--) {
        if (is_binary || ((int)*pbuf < 0x20 || (int)*pbuf > 0x7E)) {
            printf("[%02X]", *pbuf++);
        } else {
            printf("%c", *pbuf++);
        }
    }
    printf("\n");
}

//
//
//
void setTmoEventNum(const iocshArgBuf *args)
{
    char *hostname = args[0].sval;
    uint16_t port  = args[1].ival;
    char *protocol = args[2].sval;
    int num        = args[3].ival;
    struct sockaddr_in peer_addr;

    if (netDevGetHostId(hostname, &peer_addr.sin_addr.s_addr)) {
        errlogPrintf("drvNetMpf: %s: can't get hostid\n", __func__);
        return;
    }

    bool is_tcp = false;
    if (strncmp(protocol, "TCP", 3) == 0 || strncmp(protocol, "tcp", 3) == 0) {
        is_tcp = true;
    }

    PEER *p;
    epicsMutexMustLock(peerList.mutex);
    {
        for (p = (PEER *)ellFirst(&peerList.list); p; p = (PEER *)ellNext(&p->mpf.node)) {
            if ((p->mpf.peer_addr.sin_addr.s_addr == peer_addr.sin_addr.s_addr || isOmron(p->mpf.option)) &&
                p->mpf.peer_addr.sin_port == htons(port) &&
                isTcp(p->mpf.option) == is_tcp) {
                p->event_num = num;
                break;
            }
        }
    }
    epicsMutexUnlock(peerList.mutex);

    if (!p) {
        printf("peer %d not found\n", args[0].ival);
    }
}

//
//
//
void enableTmoEvent(const iocshArgBuf *args)
{
    char *hostname = args[0].sval;
    uint16_t port  = args[1].ival;
    char *protocol = args[2].sval;
    struct sockaddr_in peer_addr;

    if (netDevGetHostId(hostname, &peer_addr.sin_addr.s_addr)) {
        errlogPrintf("drvNetMpf: %s: can't get hostid\n", __func__);
        return;
    }

    bool is_tcp = false;
    if (strncmp(protocol, "TCP", 3) == 0 || strncmp(protocol, "tcp", 3) == 0) {
        is_tcp = true;
    }

    PEER *p;
    epicsMutexMustLock(peerList.mutex);
    {
        for (p = (PEER *)ellFirst(&peerList.list); p; p = (PEER *)ellNext(&p->mpf.node)) {
            if ((p->mpf.peer_addr.sin_addr.s_addr == peer_addr.sin_addr.s_addr || isOmron(p->mpf.option)) &&
                p->mpf.peer_addr.sin_port == htons(port) &&
                isTcp(p->mpf.option) == is_tcp) {
                p->tmo_event = 1;
                break;
            }
        }
    }
    epicsMutexUnlock(peerList.mutex);

    if (!p) {
        printf("peer %d not found\n", args[0].ival);
    }
}

//
//
//
void disableTmoEvent(const iocshArgBuf *args)
{
    char *hostname = args[0].sval;
    uint16_t port  = args[1].ival;
    char *protocol = args[2].sval;
    struct sockaddr_in peer_addr;

    if (netDevGetHostId(hostname, &peer_addr.sin_addr.s_addr)) {
        errlogPrintf("drvNetMpf: %s: can't get hostid\n", __func__);
        return;
    }

    bool is_tcp = false;
    if (strncmp(protocol, "TCP", 3) == 0 || strncmp(protocol, "tcp", 3) == 0) {
        is_tcp = true;
    }

    PEER *p;
    epicsMutexMustLock(peerList.mutex);
    {
        for (p = (PEER *)ellFirst(&peerList.list); p; p = (PEER *)ellNext(&p->mpf.node)) {
            if ((p->mpf.peer_addr.sin_addr.s_addr == peer_addr.sin_addr.s_addr || isOmron(p->mpf.option)) &&
                p->mpf.peer_addr.sin_port == htons(port) &&
                isTcp(p->mpf.option) == is_tcp) {
                p->tmo_event = 0;
                break;
            }
        }
    }
    epicsMutexUnlock(peerList.mutex);

    if (!p) {
        printf("peer %d not found\n", args[0].ival);
    }
}

//
//
//
void showmsg(const iocshArgBuf *args)
{
    char *hostname = args[0].sval;
    if (!hostname) {
        errlogPrintf("Usage: showmsg hostname [option]\n");
        errlogPrintf("    where:\n");
        errlogPrintf("        hostname can be IP address in dotted decimal notation\n");
        errlogPrintf("        option: 0 Comm/resp, Binary\n");
        errlogPrintf("                1 Comm/resp, Ascii\n");
        errlogPrintf("                2 Event msg, Binary\n");
        errlogPrintf("                3 Event msg, Ascii\n");
        return;
    }

    MSG_BY_IP *pm = calloc(1, sizeof(MSG_BY_IP));
    if (!pm) {
        errlogPrintf("drvNetMpf: %s: calloc failed\n", __func__);
        return;
    }

    if (netDevGetHostId(hostname, &pm->peer_addr.sin_addr.s_addr)) {
        errlogPrintf("drvNetMpf: %s: can't get hostid\n", __func__);
        return;
    }

    int32_t flags = args[1].ival;
    if (flags < 0 || flags > 3) {
        errlogPrintf("drvNetMpf: %s: option out of range\n", __func__);
        return;
    }
    if (flags & 2) {
        pm->is_event = true;
    }
    if (!(flags & 1)) {
        pm->is_binary = true;
    }

    MSG_BY_IP *p;
    epicsMutexMustLock(showmsgList.mutex);
    {
        for (p = (MSG_BY_IP *)ellFirst(&showmsgList.list); p; p = (MSG_BY_IP *)ellNext(&p->node)) {
            if (p->peer_addr.sin_addr.s_addr == pm->peer_addr.sin_addr.s_addr) {
                if (p->is_event == pm->is_event) {
                    break;
                }
            }
        }

        if (!p) {
            ellAdd(&showmsgList.list, &pm->node);
        }
    }
    epicsMutexUnlock(showmsgList.mutex);
}

//
//
//
void stopmsg(const iocshArgBuf *args)
{
    ellFree(&showmsgList.list);
}

//
//
//
void do_showmsg(MPF_COMMON *m, uint8_t *buf, int count, DIRECTION direction)
{
    epicsMutexMustLock(showmsgList.mutex);
    {
        MSG_BY_IP *pm;
        for (pm = (MSG_BY_IP *)ellFirst(&showmsgList.list);
             pm;
             pm = (MSG_BY_IP *)ellNext(&pm->node)) {
            if (pm->peer_addr.sin_addr.s_addr == m->peer_addr.sin_addr.s_addr) {
                if (pm->is_event == isEvent(m->option)) {
                    dump_msg(buf, count, direction, pm->is_binary);
                }
            }
        }
    }
    epicsMutexUnlock(showmsgList.mutex);
}

//
//
//
void showio(const iocshArgBuf *args)
{
    char *pv_name = args[0].sval;

    if (!pv_name) {
        errlogPrintf("Usage: showio pv_name\n");
        return;
    }

    MSG_BY_PV *pm = calloc(1, sizeof(MSG_BY_PV));
    if (!pm) {
        errlogPrintf("drvNetMpf: %s: calloc failed\n", __func__);
        return;
    }

    DBADDR addr;
    long status = dbNameToAddr(pv_name, &addr);
    if (status) {
        errlogPrintf("drvNetMpf: %s: dbNameToAddr failed\n", __func__);
        return;
    }
    pm->precord = addr.precord;

    epicsMutexMustLock(showioList.mutex);
    {
        MSG_BY_PV *p;
        for (p = (MSG_BY_PV *)ellFirst(&showioList.list);
             p;
             p = (MSG_BY_PV *)ellNext(&p->node)) {
            if (p->precord == pm->precord) {
                break;
            }
        }

        if (!p) {
            ellAdd(&showioList.list, &pm->node);
        }
    }
    epicsMutexUnlock(showioList.mutex);
}

//
//
//
void stopio(const iocshArgBuf *args)
{
    ellFree(&showioList.list);
}

//
//
//
void do_showio(TRANSACTION *t, int dir)
{
    static char *act[2] = {"Got", "Sending"};
    static char *row[2] = {"read", "write"};
    static char *iot[2] = {"COM/RES", "EVENT"};

    epicsMutexMustLock(showioList.mutex);
    {
        MSG_BY_PV *pm;
        for (pm = (MSG_BY_PV *)ellFirst(&showioList.list);
             pm;
             pm = (MSG_BY_PV *)ellNext(&pm->node)) {
            if (pm->precord == t->record) {
                errlogPrintf("%s message for \"%s\" to %s (%s, %s)\n",
                             act[dir],
                             pm->precord->name,
                             row[isWrite(t->option)],
                             getProtocolName(t->option),
                             iot[isEvent(t->option)]);
                break;
            }
        }
    }
    epicsMutexUnlock(showioList.mutex);
}

//
//
//
void showrtt(const iocshArgBuf *args)
{
    char *hostname = args[0].sval;
    if (!hostname) {
        errlogPrintf("Usage: showrtt hostname [option]\n");
        errlogPrintf("    where:\n");
        errlogPrintf("        hostname can be IP address in dotted decimal notation\n");
        return;
    }

    RTT_ITEM *pr = calloc(1, sizeof(RTT_ITEM));
    if (!pr) {
        errlogPrintf("drvNetMpf: %s: calloc failed\n", __func__);
        return;
    }

    if (netDevGetHostId(hostname, &pr->peer_addr.sin_addr.s_addr)) {
        errlogPrintf("drvNetMpf: %s: can't get hostid\n", __func__);
        return;
    }

    epicsMutexMustLock(showrttList.mutex);
    {
        RTT_ITEM *p;
        for (p = (RTT_ITEM *)ellFirst(&showrttList.list);
             p;
             p = (RTT_ITEM *)ellNext(&p->node)) {
            if (p->peer_addr.sin_addr.s_addr == pr->peer_addr.sin_addr.s_addr) {
                break;
            }
        }

        if (!p) {
            ellAdd(&showrttList.list, &pr->node);
        }
    }
    epicsMutexUnlock(showrttList.mutex);
}

//
//
//
void stoprtt(const iocshArgBuf *args)
{
    ellFree(&showrttList.list);
}

//
//
//
void do_showrtt(PEER *p)
{
    epicsMutexMustLock(showrttList.mutex);
    {
        RTT_ITEM *pr;
        for (pr = (RTT_ITEM *)ellFirst(&showrttList.list);
             pr;
             pr = (RTT_ITEM *)ellNext(&pr->node)) {
            if (pr->peer_addr.sin_addr.s_addr == p->mpf.peer_addr.sin_addr.s_addr) {
                char *inet_string = inet_ntoa((struct in_addr)p->mpf.peer_addr.sin_addr);
                p->recv_time.secPastEpoch--;
                p->recv_time.nsec += 1000000000;

                epicsTimeStamp diff;
                diff.secPastEpoch = p->recv_time.secPastEpoch - p->send_time.secPastEpoch;
                diff.nsec = p->recv_time.nsec - p->send_time.nsec;
                if (diff.nsec >= 1000000000) {
                    diff.nsec -= 1000000000;
                    diff.secPastEpoch++;
                }

                char time_string[256];
                epicsTimeToStrftime(time_string, sizeof(time_string), "%S.%04f", &diff);
                printf("round trip time to %s: %s\n", inet_string, time_string);
            }
        }
    }
    epicsMutexUnlock(showrttList.mutex);
}
