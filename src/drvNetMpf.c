/* drvNetMpf_314.c */
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
 * A bug in send_task has fixed.
 */

#ifndef vxWorks
#include <errno.h>
#endif

#include <dbAccess.h>
#include <dbAddr.h>
#include <dbCommon.h>
#include <epicsThread.h>
#include <recSup.h>
#include <drvSup.h>
#include <iocsh.h>
#include <epicsExport.h>

#include "drvNetMpf.h"

static int init_flag;
static int globalPeerCount;
static int globalServerCount;
static int event_server_start_flag;
static epicsTimerQueueId timerQueue;

LOCAL long create_semaphores(PEER *);
LOCAL long prepare_udp_server_socket(SERVER *);
LOCAL long spawn_io_tasks(PEER *);
LOCAL long spawn_server_task(SERVER *);
LOCAL void delete_peer(PEER *);
LOCAL void delete_server(SERVER *);
LOCAL void remove_server(SERVER *);
LOCAL TRANSACTION *get_request_from_queue(PEER *);
LOCAL int send_msg(MPF_COMMON *);
LOCAL int recv_msg(MPF_COMMON *);
LOCAL void send_task(PEER *);
LOCAL void receive_task(PEER *);
LOCAL void mpf_timeout_handler(PEER *);
LOCAL long init(void);
LOCAL long report(void);
LOCAL long spawn_tcp_parent(SERVER *);
LOCAL int tcp_parent(SERVER *);
LOCAL void reconnect(MPF_COMMON *);
LOCAL int event_server(SERVER *);
void dump_msg(uint8_t *, ssize_t, int, int);
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
void do_showmsg(MPF_COMMON *, uint8_t *, int, int);
void showio(const iocshArgBuf *);
void stopio(const iocshArgBuf *);
void do_showio(TRANSACTION *, int);
void showrtt(const iocshArgBuf *);
void stoprtt(const iocshArgBuf *);
void do_showrtt(PEER *);

long netDevGetHostId(char *hostname, in_addr_t *hostid);

#define RECONNECT (ERROR - 1)
#define MAX_CONNECT_RETRY  1

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

/*
 * Report
 */
LOCAL long report(void)
{
    printf("drvNetMpf: report() has currently nothing to do.\n");

    return OK;
}

/*
 *
 */
LOCAL long init(void)
{
    LOGMSG("drvNetMpf: init() entered\n");

    if (init_flag) {
        return OK;
    }
    init_flag = 1;

    if ((peerList.mutex = epicsMutexCreate()) == 0) {
        errlogPrintf("drvNetMpf: epicsMutexCreate failed\n");
        return ERROR;
    }

    if ((serverList.mutex = epicsMutexCreate()) == 0) {
        errlogPrintf("drvNetMpf: epicsMutexCreate failed\n");
        return ERROR;
    }

    if ((showmsgList.mutex = epicsMutexCreate()) == 0) {
        errlogPrintf("drvNetMpf: epicsMutexCreate failed\n");
        return ERROR;
    }

    if ((showioList.mutex = epicsMutexCreate()) == 0) {
      errlogPrintf("drvNetMpf: epicsMutexCreate failed\n");
      return ERROR;
    }

    if ((showrttList.mutex = epicsMutexCreate()) == 0) {
        errlogPrintf("drvNetMpf: epicsMutexCreate failed\n");
        return ERROR;
    }

    ellInit(&peerList.list);
    ellInit(&serverList.list);
    ellInit(&showmsgList.list);
    ellInit(&showioList.list);
    ellInit(&showrttList.list);

    if (!(timerQueue = epicsTimerQueueAllocate(1, epicsThreadPriorityScanHigh))) {
        errlogPrintf("drvNetMpf: epicsTimerQueueAllocate failed\n");
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

/*
 * Creates semaphores
 */
LOCAL long create_semaphores(PEER *p) {
    LOGMSG("drvNetMpf: create_semaphores(%8p)\n", p);

    if ((p->in_t_mutex=epicsMutexCreate()) == 0) {
        errlogPrintf("drvNetMpf: epicsMutexCreate failed\n");
        return ERROR;
    }

    if ((p->reqQ_mutex=epicsMutexCreate()) == 0) {
        errlogPrintf("drvNetMpf: epicsMutexCreate failed\n");
        return ERROR;
    }

    if ((p->req_queued=epicsEventCreate(epicsEventEmpty)) == 0) {
        errlogPrintf("drvNetMpf: epicsEventCreate failed\n");
        return ERROR;
    }

    if ((p->next_cycle=epicsEventCreate(epicsEventEmpty)) == 0) {
        errlogPrintf("drvNetMpf: epicsEventCreate failed\n");
        return ERROR;
    }

    return OK;
}

/*
 * Spawn I/O tasks
 */
LOCAL long spawn_io_tasks(PEER *p)
{
    char *send_t_name = "tSndTsk";
    char *recv_t_name = "tRcvTsk";
    char  task_name[32];

    LOGMSG("drvNetMpf: spawn_io_tasks(%8p)\n", p);

    sprintf(task_name, "%s_%d", send_t_name, p->mpf.id);

    if ((p->send_tid= epicsThreadCreate(task_name,
                                        SEND_PRIORITY,
                                        SEND_STACK,
                                        (EPICSTHREADFUNC)send_task,
                                        p
                                        )) == 0) {
        p->send_tid = 0;
        errlogPrintf("drvNetMpf: epicsThreadCreate failed\n");
        return ERROR;
    }

    sprintf(task_name, "%s_%d", recv_t_name, p->mpf.id);

    if ((p->recv_tid=epicsThreadCreate(task_name,
                                       RECV_PRIORITY,
                                       RECV_STACK,
                                       (EPICSTHREADFUNC)receive_task,
                                       p
                                       )) == 0) {
        p->recv_tid = 0;
        errlogPrintf("drvNetMpf: epicsThreadCreate failed\n");
        return ERROR;
    }

    return OK;
}

/*
 * Deletes peer
 * This function should be called only before
 * the peer is added to the peerList.
 */
LOCAL void delete_peer(PEER *p)
{
    LOGMSG("drvNetMpf: delete_peer(%8p)\n", p);

    if (p->mpf.sfd) {
        close(p->mpf.sfd);
    }
    /*
      if (p->wd_id)epicsTimerDestroy(p->wd_id);
    */
    if (p->next_cycle) {
        epicsEventDestroy(p->next_cycle);
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

/*
 * Creates and initialize peer structure
 */
PEER *drvNetMpfInitPeer(struct sockaddr_in peer_addr, int option)
{
    if (!init_flag) {
        errlogPrintf("drvNetMpf: not initialized, check xxxAppInclude.dbd\n");
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
                errlogPrintf("drvNetMpf: peer option does not match\n");
                epicsMutexUnlock(peerList.mutex);
                return NULL;
            }

            epicsMutexUnlock(peerList.mutex);
            return p;
        }

        p = calloc(1, sizeof(PEER));

        if (!p) {
            errlogPrintf("drvNetMpf: calloc failed\n");
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
            errlogPrintf("drvNetMpf: create_semaphores failed\n");
            delete_peer(p);
            epicsMutexUnlock(peerList.mutex);
            return NULL;
        }

        if (!(p->wd_id = epicsTimerQueueCreateTimer(timerQueue,
                                                    (epicsTimerCallback)mpf_timeout_handler,
                                                    p
                                                    ))) {
            errlogPrintf("drvNetMpf: epicsTimerQueueCreateTimer failed\n");
            delete_peer(p);
            epicsMutexUnlock(peerList.mutex);
            return NULL;
        }

        LOGMSG("drvNetMpf: send buf size %d\n", SEND_BUF_SIZE(p->mpf.option));
        LOGMSG("drvNetMpf: recv buf size %d\n", RECV_BUF_SIZE(p->mpf.option));

        p->mpf.send_buf = calloc(1, SEND_BUF_SIZE(p->mpf.option));
        if (!p->mpf.send_buf) {
            errlogPrintf("drvNetMpf: calloc failed (send_buf)\n");
            delete_peer(p);
            epicsMutexUnlock(serverList.mutex);
            return NULL;
        }

        p->mpf.recv_buf = calloc(1, RECV_BUF_SIZE(p->mpf.option));
        if (!p->mpf.recv_buf) {
            errlogPrintf("drvNetMpf: calloc failed (recv_buf)\n");
            delete_peer(p);
            epicsMutexUnlock(serverList.mutex);
            return NULL;
        }

        if (spawn_io_tasks(p) == ERROR) {
            errlogPrintf("drvNetMpf: spawn_io_tasks failed\n");
            delete_peer(p);
            epicsMutexUnlock(peerList.mutex);
            return NULL;
        }

        ellAdd(&peerList.list, &p->mpf.node);
    }
    epicsMutexUnlock(peerList.mutex);

    return p;
}

/*
 * Send request
 */
long drvNetMpfSendRequest(TRANSACTION *t)
{
    LOGMSG("drvNetMpf: drvNetMpfSendRequest(%8p)\n", t);

    if (!t) {
        errlogPrintf("drvNetMpf: null request\n");
        return ERROR;
    }

    PEER *p;
    if (!(p = (PEER *)t->facility)) {
        errlogPrintf("drvNetMpf: null peer\n");
        return ERROR;
    }

    epicsMutexMustLock(p->reqQ_mutex);
    {
        if (isLast(t->option)) {
            ellAdd(&p->reqQueue, &t->node);
        } else {
            ellInsert(&p->reqQueue, NULL, &t->node);
        }
    }
    epicsMutexUnlock(p->reqQ_mutex);

    epicsEventSignal(p->req_queued);

    return OK;
}

/*
 * Get request from queue
 */
LOCAL TRANSACTION *get_request_from_queue(PEER *p)
{
    TRANSACTION *t;
    epicsMutexMustLock(p->reqQ_mutex);
    {
        t = (TRANSACTION *)ellGet(&p->reqQueue);
    }
    epicsMutexUnlock(p->reqQ_mutex);

    return t;
}

/*
 * Send message
 */
LOCAL int send_msg(MPF_COMMON *m)
{
    void *cur = m->send_buf;

    if (isUdp(m->option)) {
        ssize_t len = sendto(m->sfd,
                             cur,
                             m->send_len,
                             0,
                             &m->peer_addr,
                             sizeof(m->peer_addr)
                             );
        if (len != m->send_len) {
            errlogPrintf("drvNetMpf: sendto() failed [%d]", errno);
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
                errlogPrintf("drvNetMpf: send error [%d]\n", errno);
                return RECONNECT;
            }

            m->send_len -= len;
            cur += len;
        }
    }

    m->send_len = cur - m->send_buf;

    DUMP_MSG(m, m->send_buf, cur - m->send_buf, 1);
    do_showmsg(m, m->send_buf, cur - m->send_buf, 1);

    return OK;
}

/*
 * Reconnect
 */
LOCAL void reconnect(MPF_COMMON *m)
{
    LOGMSG("drvNetMpf: reconnect(%8p)\n", m);

#ifdef vxWorks
    char inet_string[256];
    inet_ntoa_b((struct in_addr)m->peer_addr.sin_addr, inet_string);
#else
    char *inet_string = inet_ntoa((struct in_addr)m->peer_addr.sin_addr);
#endif

    while ((m->sfd = socket(AF_INET,
                            isUdp(m->option) ?
                            SOCK_DGRAM : SOCK_STREAM,
                            0
                            )) == ERROR) {
        errlogPrintf("drvNetMpf: socket failed[%d]\n", errno);
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
            errlogPrintf("drvNetMpf: Omron was detected [port: %d]\n",
                         ntohs(my_addr.sin_port));
        }

        while (bind(m->sfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) == ERROR) {
            errlogPrintf("drvNetMpf: bind failed[%d]\n", errno);
            epicsThreadSleep(1.0);
        }
    } else {
        int true = TRUE;
        /*
         * turn on KEEPALIVE so if the client system crashes
         * this task will find out and suspend
         */
        while (setsockopt(m->sfd,
                          SOL_SOCKET,
                          SO_KEEPALIVE,
                          &true,
                          sizeof(true)
                          ) == ERROR) {
            errlogPrintf("setsockopt(SO_KEEPALIVE) failed\n");
            epicsThreadSleep(1.0);
        }

        do {
            errlogPrintf("drvNetMpf: tcp client trying to connect to \"%s\"...\n", inet_string);
            epicsThreadSleep(1.0);
        } while (connect(m->sfd, (struct sockaddr *)&m->peer_addr, (socklen_t)sizeof(m->peer_addr)) == ERROR);

        errlogPrintf("drvNetMpf: connected to \"%s\"\n", inet_string);

        setReconn(m->option);
    }
}

/*
 * Receive message
 */
LOCAL int recv_msg(MPF_COMMON *m)
{
    void *cur = m->recv_buf;
    void *end = m->recv_buf + RECV_BUF_SIZE(m->option);

    memset(cur, 0, RECV_BUF_SIZE(m->option));

    if (isUdp(m->option)) {
        socklen_t sender_addr_len = sizeof(m->sender_addr);
        memset(&m->sender_addr, 0, sender_addr_len);

        if (m->recv_len > RECV_BUF_SIZE(m->option)) {
            errlogPrintf("drvNetMpf: receive buffer running short (requested:%zd, max:%d)\n",
                         m->recv_len, RECV_BUF_SIZE(m->option));
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
            errlogPrintf("drvNetMpf: recvfrom() error [%d]\n", errno);
            return ERROR;
        }

        cur += len;
    } else {
        do {
            if (m->recv_len > end - cur) {
                errlogPrintf("drvNetMpf: receive buffer running short (requested:%zd, max:%zd)\n",
                             m->recv_len, end - cur);
                return ERROR;
            }

            ssize_t len = recv(m->sfd,
                               cur,
                               m->recv_len ? m->recv_len : RECV_BUF_SIZE(m->option),
                               0
                               );
            if (len == ERROR) {
                errlogPrintf("drvNetMpf: recv() error [%d]\n", errno);
                return RECONNECT;
            } else if (len == 0) {
                errlogPrintf("drvNetMpf: EOF found while receiving\n");
                return RECONNECT;
            }

            m->recv_len -= len;
            cur += len;
        } while (m->recv_len > 0);
    }

    m->recv_len = cur - m->recv_buf;

    DUMP_MSG(m, m->recv_buf, cur - m->recv_buf, 0);
    do_showmsg(m, m->recv_buf, cur - m->recv_buf, 0);

    return OK;
}

/*
 * Send task
 */
LOCAL void send_task(PEER *p)
{
    for (;;) {
        epicsEventMustWait(p->req_queued);

        TRANSACTION *t;
        while ((t = get_request_from_queue(p))) {
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
                setLast(t->option);
                callbackRequest(&t->io.async.callback);
                continue;
            }

            if (ret >= 0) {
                p->mpf.recv_len = ret;
            }

            if (!t->parse_response || ret == NO_RESP) {
                do_showio(t, 1);
                t->ret = send_msg(&p->mpf);
                if (!t->ret && ret == NOT_DONE) {
                    drvNetMpfSendRequest(t);
                } else {
                    setLast(t->option);
                    callbackRequest(&t->io.async.callback);
                }

                continue;
            }
#ifdef SANITY_CHECK
            {
                /* sanity check */
                uint32_t delta =
                        t->io.async.cancel_counter + t->io.async.receive_counter +
                        t->io.async.timeout_counter - t->io.async.send_counter;

                if (delta) {
                    errlogPrintf("drvNetMpf: doubled callback detected (delta:%d)\n", delta);
                }
#ifdef SANITY_PRINT
                else if (!(t->io.async.send_counter % 10000)) {
                    static int i = 0;

                    errlogPrintf("drvNetMpf: sanity check OK (%d times)\n", i++);
                    errlogPrintf("cancel : %d times\n", t->io.async.cancel_counter);
                    errlogPrintf("receive: %d times\n", t->io.async.receive_counter);
                    errlogPrintf("timeout: %d times\n", t->io.async.timeout_counter);
                    errlogPrintf("send   : %d times\n", t->io.async.send_counter);
                    errlogPrintf("delta  : %d times\n", delta);
                }
#endif /* SANITY_PRINT */
            }
#endif /* SANITY_CHECK */

            t->io.async.timeout_flag = 0;
            p->in_transaction = t;
            t->io.async.send_counter++;
            epicsTimerStartDelay(p->wd_id, t->io.async.timeout);
            do_showio(t, 1);
            if ((t->ret = send_msg(&p->mpf))) {
                epicsTimerCancel(p->wd_id);
                epicsMutexMustLock(p->in_t_mutex);
                {
                    t = p->in_transaction;
                    p->in_transaction = 0;
                }
                epicsMutexUnlock(p->in_t_mutex);

                if (t) {
                    setLast(t->option);
                    callbackRequest(&t->io.async.callback);
                    t->io.async.cancel_counter++;
                }

                continue;
            }

            epicsTimeGetCurrent(&p->send_time);
            epicsEventMustWait(p->next_cycle);
        }
    }
}

/*
 * Receve task
 */
LOCAL void receive_task(PEER *p)
{
    reconnect(&p->mpf);

    for (;;) {
        if (recv_msg(&p->mpf) == RECONNECT) {
            shutdown(p->mpf.sfd, 2);
            close(p->mpf.sfd);
            epicsThreadSleep(1.0);
            reconnect(&p->mpf);
            continue;
        }

        TRANSACTION *t;
        epicsTimerCancel(p->wd_id);
        epicsMutexMustLock(p->in_t_mutex);
        {
            t = p->in_transaction;
            p->in_transaction = 0;
        }
        epicsMutexUnlock(p->in_t_mutex);

        if (t) {
            t->ret = t->parse_response(t->record,
                                       &t->option,
                                       p->mpf.recv_buf,
                                       &p->mpf.recv_len,
                                       t->device,
                                       t->transaction_id
                                       );
            if (t->ret != NOT_MINE) {
                do_showio(t, 0);
                if (t->ret == RCV_MORE) {
                    p->in_transaction = t;
                    epicsTimerStartDelay(p->wd_id, t->io.async.timeout);
                    continue;
                }

                t->io.async.receive_counter++;

                if (t->ret == NOT_DONE) {
                    drvNetMpfSendRequest(t);
                } else {
                    LOGMSG("drvNetMpf: requesting callback for \"%s\"\n", t->record->name);

                    setLast(t->option);
                    callbackRequest(&t->io.async.callback);
                }
                epicsTimeGetCurrent(&p->recv_time);
                do_showrtt(p);
                epicsEventSignal(p->next_cycle);
            } else {
                p->in_transaction = t;
                errlogPrintf("drvNetMpf: discarding unexpected response for \"%s\"\n", t->record->name);
                epicsTimerStartDelay(p->wd_id, t->io.async.timeout);
            }
        } else {
            errlogPrintf("drvNetMpf: discarding stray response...\n");
        }

        p->mpf.recv_len = 0;
    }
}

/*
 * Timeout handler
 */
LOCAL void mpf_timeout_handler(PEER *p)
{
    TRANSACTION *t;
    epicsMutexMustLock(p->in_t_mutex);
    {
        t = p->in_transaction;
        p->in_transaction = 0;
    }
    epicsMutexUnlock(p->in_t_mutex);

    if (t) {
        epicsTimeStamp current;
        char time[256];

        epicsTimeGetCurrent(&current);
        epicsTimeToStrftime(time, sizeof(time), "%Y.%m/%d %H:%M:%S.%06f", &current);
        errlogPrintf("drvNetMpf: *** mpf_timeout_handler(\"%s\"):%s ***\n", t->record->name, time);

        t->io.async.timeout_flag = 1;
        t->io.async.timeout_counter++;
        setLast(t->option);
        callbackRequest(&t->io.async.callback);
        epicsEventSignal(p->next_cycle);

        if (p->tmo_event) {
            post_event(p->event_num);

            errlogPrintf("drvNetMpf: ###### tmo event(%d) issued ######\n", p->event_num);
        }
    }
}

/*
 * Register event
 */
long drvNetMpfRegisterEvent(TRANSACTION *t)
{
    LOGMSG("drvNetMpf: drvNetMpfRegisterEvent(%8p)\n", t);

    if (!t) {
        errlogPrintf("drvNetMpf: null event\n");
        return ERROR;
    }

    SERVER *s;
    if (!(s = (SERVER *)t->facility)) {
        errlogPrintf("drvNetMpf: null server\n");
        return ERROR;
    }

    epicsMutexMustLock(s->eventQ_mutex);
    {
        ellAdd(&s->eventQueue, &t->node);
    }
    epicsMutexUnlock(s->eventQ_mutex);

    return OK;
}

LOCAL int tcp_parent(SERVER *s)
{
    if ((s->mpf.sfd = socket(AF_INET,
                             SOCK_STREAM,
                             0
                             )) == ERROR) {
        errlogPrintf("socket creation error\n");
        return ERROR;
    }

    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family      = AF_INET;
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr.sin_port        = htons(s->port);

    for (;;) {
        errlogPrintf("drvNetMpf: tcp parent: trying to bind...\n");

        if (bind(s->mpf.sfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) == OK) {
            break;
        }

        epicsThreadSleep(10.0);
    }

    if (listen(s->mpf.sfd, 10) == ERROR) {
        errlogPrintf("listen error\n");
        close(s->mpf.sfd);
        return ERROR;
    }

    while (TRUE) {
        socklen_t sender_addr_len = sizeof(s->mpf.sender_addr);
        memset(&s->mpf.sender_addr, 0, sender_addr_len);

        int new_sfd = accept(s->mpf.sfd, (struct sockaddr *)&s->mpf.sender_addr, &sender_addr_len);
        if (new_sfd == ERROR) {
            errlogPrintf("client accept error [%d]\n", errno);
            epicsThreadSleep(15.0);
            continue;
        }

        /*
         * turn on KEEPALIVE so if the client system crashes
         * this task will find out and suspend
         */
        int true = TRUE;
        if (setsockopt(new_sfd,
                       SOL_SOCKET,
                       SO_KEEPALIVE,
                       &true,
                       sizeof(true)
                       ) == ERROR) {
            errlogPrintf("setsockopt(SO_KEEPALIVE) failed\n");
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

#ifdef vxWorks
        char visitor[256];
        inet_ntoa_b((struct in_addr)s->mpf.sender_addr.sin_addr, visitor);
#else
        char *visitor = inet_ntoa((struct in_addr)s->mpf.sender_addr.sin_addr);
#endif
        if (!t) {
            errlogPrintf("drvNetMpf: unexpected connection request from %s\n", visitor);
            close(new_sfd);
            continue; /* while (TRUE) */
        }

        errlogPrintf("drvNetMpf: connection request from %s\n", visitor);

        SERVER *c = calloc(1, sizeof(SERVER));
        if (!c) {
            errlogPrintf("drvNetMpf: calloc failed\n");
            close(new_sfd);
            continue; /* while (TRUE) */
        }

        c->mpf.sfd = new_sfd;
        c->mpf.id = globalServerCount++;
        c->mpf.option = s->mpf.option;
        c->parent = s;
        ellInit(&c->eventQueue);

        if ((c->eventQ_mutex = epicsMutexCreate()) == 0) {
            errlogPrintf("drvNetMpf: epicsMutexCreate failed\n");
            delete_server(c);
            continue; /* while (TRUE) */
        }

        epicsMutexMustLock(c->eventQ_mutex);
        {
            ellAdd(&c->eventQueue, &t->node);
        }
        epicsMutexUnlock(c->eventQ_mutex);

        c->mpf.send_buf = calloc(1, SEND_BUF_SIZE(c->mpf.option));
        if (!c->mpf.send_buf) {
            errlogPrintf("drvNetMpf: calloc failed (send_buf)\n");
            delete_server(c);
            continue; /* while (TRUE) */
        }

        c->mpf.recv_buf = calloc(1, RECV_BUF_SIZE(c->mpf.option));
        if (!c->mpf.recv_buf) {
            errlogPrintf("drvNetMpf: calloc failed (recv_buf)\n");
            delete_server(c);
            continue; /* while (TRUE) */
        }

        if (spawn_server_task(c) == ERROR) {
            errlogPrintf("drvNetMpf: spawn_sever_tasks failed\n");
            delete_server(c);
            continue; /* while (TRUE) */
        }

        epicsMutexMustLock(serverList.mutex);
        {
            ellAdd(&serverList.list, &c->mpf.node);
        }
        epicsMutexUnlock(serverList.mutex);
    }
}

/*
 * Create TCP parent
 */
LOCAL long spawn_tcp_parent(SERVER *s)
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
        errlogPrintf("drvNetMpf: epicsThreadCreate failed\n");
        return ERROR;
    }

    return OK;
}

/*
 * Creates socket and connect
 */
LOCAL long prepare_udp_server_socket(SERVER *s)
{
    LOGMSG("drvNetMpf: prepare_udp_server_socket(%8p)\n", s);

    if ((s->mpf.sfd = socket(AF_INET,
                             SOCK_DGRAM,
                             0
                             )) == ERROR) {
        errlogPrintf("drvNetMpf: socket failed[%d]\n", errno);
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
        errlogPrintf("drvNetMpf: bind failed[%d]\n", errno);
        return ERROR;
    }

    return OK;
}

/*
 * Spawn server task
 */
LOCAL long spawn_server_task(SERVER *s)
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
        errlogPrintf("drvNetMpf: epicsThreadCreate failed\n");
        return ERROR;
    }

    return OK;
}

/*
 * Deletes server
 */
LOCAL void delete_server(SERVER *s)
{
    /*
     * This function should be called only before
     * the server is added to the serverList.
     */
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

/*
 * Creates and initialize server structure
 */
SERVER *drvNetMpfInitServer(unsigned short port, int option)
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
                errlogPrintf("drvNetMpf: server option dose not match\n");
                epicsMutexUnlock(serverList.mutex);
                return NULL;
            }

            /*
             * only one (parent) server for a specific port
             */
            epicsMutexUnlock(serverList.mutex);
            return s;
        }

        s = calloc(1, sizeof(SERVER));
        if (!s) {
            errlogPrintf("drvNetMpf: calloc failed\n");
            epicsMutexUnlock(serverList.mutex);
            return NULL;
        }

        s->port = port;
        s->mpf.id = globalServerCount++;
        s->mpf.option = GET_MPF_OPTION(option);
        ellInit(&s->eventQueue);

        if ((s->eventQ_mutex=epicsMutexCreate()) == 0) {
            errlogPrintf("drvNetMpf: epicsMutexCreate failed\n");
            delete_server(s);
            epicsMutexUnlock(serverList.mutex);
            return NULL;
        }

        if (isTcp(option)) {
            if (spawn_tcp_parent(s)) {
                errlogPrintf("drvNetMpf: spawn_tcp_parent failed\n");
                delete_server(s);
                epicsMutexUnlock(serverList.mutex);
                return NULL;
            }
        } else {
            if (prepare_udp_server_socket(s) == ERROR) {
                errlogPrintf("drvNetMpf: prepare_udp_server_socket failed\n");
                delete_server(s);
                epicsMutexUnlock(serverList.mutex);
                return NULL;
            }

            s->mpf.send_buf = calloc(1, SEND_BUF_SIZE(s->mpf.option));
            if (!s->mpf.send_buf) {
                errlogPrintf("drvNetMpf: calloc failed (send_buf)\n");
                delete_server(s);
                epicsMutexUnlock(serverList.mutex);
                return NULL;
            }

            s->mpf.recv_buf = calloc(1, RECV_BUF_SIZE(s->mpf.option));
            if (!s->mpf.recv_buf) {
                errlogPrintf("drvNetMpf: calloc failed (recv_buf)\n");
                delete_server(s);
                epicsMutexUnlock(serverList.mutex);
                return NULL;
            }

            if (spawn_server_task(s) == ERROR) {
                errlogPrintf("drvNetMpf: spawn_server_task failed\n");
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

/*
 * Searches for event acceptor
 */
LOCAL int event_server(SERVER *s)
{
    for (;;) {
        if (!event_server_start_flag) {
            errlogPrintf("drvNetMpf: event server %d waiting for getting started...\n", s->mpf.id);
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
            shutdown(s->mpf.sfd, 2);
            close(s->mpf.sfd);
            errlogPrintf("drvNetMpf: event server %d exitting...\n", s->mpf.id);
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

/*
 * remove server
 */
LOCAL void remove_server(SERVER *target)
{
    SERVER *s;
    epicsMutexMustLock(serverList.mutex);
    {
        for (s = (SERVER *)ellFirst(&serverList.list); s; s = (SERVER *)ellNext(&s->mpf.node)) {
            if (s == target) {
                ellDelete(&serverList.list, &s->mpf.node);
                errlogPrintf("server: %d removed\n", s->mpf.id);
                break;
            }
        }
    }
    epicsMutexUnlock(serverList.mutex);

    if (s) {
        delete_server(s);
    }
}

/*
 * Prints diagnosis facilities
 */
void startEventServer(const iocshArgBuf *args)
{
    event_server_start_flag = 1;
}

/*
 * Prints diagnosis facilities
 */
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

/*
 * Shows peer info
 */
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
#ifdef vxWorks
        char inet_string[256];
        inet_ntoa_b((struct in_addr)p->mpf.peer_addr.sin_addr, inet_string);
#else
        char *inet_string = inet_ntoa(p->mpf.peer_addr.sin_addr);
#endif
        printf("  ------------------------------\n");
        printf("  id:                %d\n", p->mpf.id);
        printf("  protocol:          %s\n", isUdp(p->mpf.option) ? "UDP" : "TCP");
        printf("  socket_fd:         %d\n", p->mpf.sfd);
        printf("  addrss_family:     0x%04x\n", p->mpf.peer_addr.sin_family);
        printf("  intenet_addr:      %s\n", inet_string);
        printf("  port_number:       0x%04x\n", ntohs(p->mpf.peer_addr.sin_port));
        printf("  in_transaction:    %8p\n", p->in_transaction);
        printf("  in_t_mutex_sem_id: %8p\n", p->in_t_mutex);
        printf("  num_requests:      %d\n", ellCount(&p->reqQueue));
        printf("  reqQ_mutex_sem_id: %8p\n", p->reqQ_mutex);
        printf("  req_queued_sem_id: %8p\n", p->req_queued);
        printf("  next_cycle_sem_id: %8p\n", p->next_cycle);
        printf("  watchdog_id:       %8p\n", p->wd_id);
        printf("  send_task_id:      %8p\n", p->send_tid);
        printf("  recv_task_id:      %8p\n", p->recv_tid);
        printf("  send_msg_len:      %zd\n", p->mpf.send_len);
        printf("  recv_msg_len:      %zd\n", p->mpf.recv_len);
        printf("  send_buf_addr:     %8p\n", p->mpf.send_buf);
        printf("  recv_buf_addr:     %8p\n", p->mpf.recv_buf);
    }

    if (args[1].ival > 1) {
        /*
          d(p->mpf.recv_buf, 0, 1);
          printf("\n\n\n");
        */
    }
}

/*
 * peerShow for all peers
 */
void peerShowAll(const iocshArgBuf *args)
{
    PEER *p;
    epicsMutexMustLock(peerList.mutex);
    {
        for (p = (PEER *)ellFirst(&peerList.list); p; p = (PEER *)ellNext(&p->mpf.node)) {
#ifdef vxWorks
            char inet_string[256];
            inet_ntoa_b((struct in_addr)p->mpf.peer_addr.sin_addr, inet_string);
#else
            char *inet_string = inet_ntoa((struct in_addr)p->mpf.peer_addr.sin_addr);
#endif
            char *protocol = isUdp(p->mpf.option) ? "UDP" : "TCP";
            uint16_t port = ntohs(p->mpf.peer_addr.sin_port);

            printf("Peer %d: %s %d(0x%x)/%s\n", p->mpf.id, inet_string, port, port, protocol);
        }
    }
    epicsMutexUnlock(peerList.mutex);
}

/*
 * Shows server info
 */
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
#ifdef vxWorks
        char inet_string[256];
        inet_ntoa_b((struct in_addr)s->mpf.sender_addr.sin_addr, inet_string);
#else
        char *inet_string = inet_ntoa(s->mpf.sender_addr.sin_addr);
#endif
        printf("  ------------------------------\n");
        printf("  id:                  %d\n", s->mpf.id);
        printf("  protocol:            %s\n", isUdp(s->mpf.option) ? "UDP" : "TCP");
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

    if (args[1].ival > 1) {
        /*
          d(s->mpf.recv_buf, 0, 1);
          printf("\n\n\n");
        */
    }
}

/*
 * serverShow for all servers
 */
void serverShowAll(const iocshArgBuf *args)
{
    SERVER *s;

    epicsMutexMustLock(serverList.mutex);
    {
        for (s = (SERVER *)ellFirst(&serverList.list); s; s = (SERVER *)ellNext(&s->mpf.node)) {
#ifdef vxWorks
            char inet_string[256];
            inet_ntoa_b((struct in_addr)s->mpf.sender_addr.sin_addr, inet_string);
#else
            char *inet_string = inet_ntoa((struct in_addr)s->mpf.sender_addr.sin_addr);
#endif
            char *protocol = isUdp(s->mpf.option) ? "UDP" : "TCP";
            printf("Server %d: %s %d(0x%x)/%s\n",
                   s->mpf.id, inet_string, s->port, s->port, protocol);
        }
    }
    epicsMutexUnlock(serverList.mutex);
}

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

/*
 * Dumps message
 */
void dump_msg(uint8_t *pbuf, ssize_t count, int dir, int flag)
{
    if (dir) {
        printf("=>");
    } else {
        printf("<=");
    }

    while (count--) {
        if (flag == 1 || ((int)*pbuf < 0x20 || (int)*pbuf > 0x7E)) {
            printf("[%02X]", *pbuf++);
        } else {
            printf("%c", *pbuf++);
        }
    }
    printf("\n");
}

void setTmoEventNum(const iocshArgBuf *args)
{
    char *hostname = args[0].sval;
    uint16_t port  = args[1].ival;
    char *protocol = args[2].sval;
    int num        = args[3].ival;
    struct sockaddr_in peer_addr;

    if (netDevGetHostId(hostname, &peer_addr.sin_addr.s_addr)) {
        errlogPrintf("drvNetMpf: can't get hostid\n");
        return;
    }

    int option = 0;
    if (strncmp(protocol, "TCP", 3) == 0 || strncmp(protocol, "tcp", 3) == 0) {
        option |= MPF_TCP;
    }

    PEER *p;
    epicsMutexMustLock(peerList.mutex);
    {
        for (p = (PEER *)ellFirst(&peerList.list); p; p = (PEER *)ellNext(&p->mpf.node)) {
            if ((p->mpf.peer_addr.sin_addr.s_addr == peer_addr.sin_addr.s_addr || isOmron(option)) &&
                p->mpf.peer_addr.sin_port == htons(port) &&
                GET_PROTOCOL(p->mpf.option) == GET_PROTOCOL(option)) {
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

void enableTmoEvent(const iocshArgBuf *args)
{
    char *hostname = args[0].sval;
    uint16_t port  = args[1].ival;
    char *protocol = args[2].sval;
    struct sockaddr_in peer_addr;

    if (netDevGetHostId(hostname, &peer_addr.sin_addr.s_addr)) {
        errlogPrintf("drvNetMpf: can't get hostid\n");
        return;
    }

    int option = 0;
    if (strncmp(protocol, "TCP", 3) == 0 || strncmp(protocol, "tcp", 3) == 0) {
        option |= MPF_TCP;
    }

    PEER *p;
    epicsMutexMustLock(peerList.mutex);
    {
        for (p = (PEER *)ellFirst(&peerList.list); p; p = (PEER *)ellNext(&p->mpf.node)) {
            if ((p->mpf.peer_addr.sin_addr.s_addr == peer_addr.sin_addr.s_addr || isOmron(option)) &&
                p->mpf.peer_addr.sin_port == htons(port) &&
                GET_PROTOCOL(p->mpf.option) == GET_PROTOCOL(option)) {
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

void disableTmoEvent(const iocshArgBuf *args)
{
    char *hostname = args[0].sval;
    uint16_t port  = args[1].ival;
    char *protocol = args[2].sval;
    struct sockaddr_in peer_addr;

    if (netDevGetHostId(hostname, &peer_addr.sin_addr.s_addr)) {
        errlogPrintf("drvNetMpf: can't get hostid\n");
        return;
    }

    int option = 0;
    if (strncmp(protocol, "TCP", 3) == 0 || strncmp(protocol, "tcp", 3) == 0) {
        option |= MPF_TCP;
    }

    PEER *p;
    epicsMutexMustLock(peerList.mutex);
    {
        for (p = (PEER *)ellFirst(&peerList.list); p; p = (PEER *)ellNext(&p->mpf.node)) {
            if ((p->mpf.peer_addr.sin_addr.s_addr == peer_addr.sin_addr.s_addr || isOmron(option)) &&
                p->mpf.peer_addr.sin_port == htons(port) &&
                GET_PROTOCOL(p->mpf.option) == GET_PROTOCOL(option)) {
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
        errlogPrintf("drvNetMpf: calloc failed\n");
        return;
    }

    if (netDevGetHostId(hostname, &pm->peer_addr.sin_addr.s_addr)) {
        errlogPrintf("drvNetMpf: can't get hostid\n");
        return;
    }

    int option = args[1].ival;
    if (option < 0 || option > 3) {
        errlogPrintf("drvNetMpf: option out of range\n");
        return;
    }
    if (option & 2) {
        pm->option |= MPF_EVENT;
    }
    if (!(option & 1)) {
        pm->is_bin = 1;
    }

    MSG_BY_IP *p;
    epicsMutexMustLock(showmsgList.mutex);
    {
        for (p = (MSG_BY_IP *)ellFirst(&showmsgList.list);
             p;
             p = (MSG_BY_IP *)ellNext(&p->node)) {
            if (p->peer_addr.sin_addr.s_addr == pm->peer_addr.sin_addr.s_addr) {
                if (p->option == (pm->option & EVENT_MASK)) {
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

void stopmsg(const iocshArgBuf *args)
{
    ellFree(&showmsgList.list);
}

void do_showmsg(MPF_COMMON *m, uint8_t *buf, int count, int dir)
{
    epicsMutexMustLock(showmsgList.mutex);
    {
        MSG_BY_IP *pm;
        for (pm = (MSG_BY_IP *)ellFirst(&showmsgList.list);
             pm;
             pm = (MSG_BY_IP *)ellNext(&pm->node)) {
            if (pm->peer_addr.sin_addr.s_addr == m->peer_addr.sin_addr.s_addr) {
                if (pm->option == (m->option & EVENT_MASK)) {
                    dump_msg(buf, count, dir, pm->is_bin);
                }
            }
        }
    }
    epicsMutexUnlock(showmsgList.mutex);
}

void showio(const iocshArgBuf *args)
{
    char *pv_name = args[0].sval;

    if (!pv_name) {
        errlogPrintf("Usage: showio pv_name\n");
        return;
    }

    MSG_BY_PV *pm = calloc(1, sizeof(MSG_BY_PV));
    if (!pm) {
        errlogPrintf("drvNetMpf: calloc failed\n");
        return;
    }

    DBADDR addr;
    long status = dbNameToAddr(pv_name, &addr);
    if (status) {
        errlogPrintf("drvNetMpf: dbNameToAddr failed\n");
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

void stopio(const iocshArgBuf *args)
{
    ellFree(&showioList.list);
}


void do_showio(TRANSACTION *t, int dir)
{
    static char *act[2] = {"Got", "Sending"};
    static char *row[2] = {"read", "write"};
    static char *pro[2] = {"UDP", "TCP"};
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
                             pro[isTcp(t->option)],
                             iot[isEvent(t->option)]);
                break;
            }
        }
    }
    epicsMutexUnlock(showioList.mutex);
}

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
        errlogPrintf("drvNetMpf: calloc failed\n");
        return;
    }

    if (netDevGetHostId(hostname, &pr->peer_addr.sin_addr.s_addr)) {
        errlogPrintf("drvNetMpf: can't get hostid\n");
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

void stoprtt(const iocshArgBuf *args)
{
    ellFree(&showrttList.list);
}


void do_showrtt(PEER *p)
{
    epicsMutexMustLock(showrttList.mutex);
    {
        RTT_ITEM *pr;
        for (pr = (RTT_ITEM *)ellFirst(&showrttList.list);
             pr;
             pr = (RTT_ITEM *)ellNext(&pr->node)) {
            if (pr->peer_addr.sin_addr.s_addr == p->mpf.peer_addr.sin_addr.s_addr) {
#ifdef vxWorks
                char inet_string[256];
                inet_ntoa_b((struct in_addr)p->mpf.peer_addr.sin_addr, inet_string);
#else
                char *inet_string = inet_ntoa((struct in_addr)p->mpf.peer_addr.sin_addr);
#endif
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
