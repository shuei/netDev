/* devNetDev_314.c */
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

#include        <netdb.h>

#include        <alarm.h>
#include        <recSup.h>
#include        <recGbl.h>
#include        <dbLock.h>
#include        <dbFldTypes.h>
#include        "drvNetMpf.h"
#include        "devNetDev.h"

static int init_flag = 0;

LOCAL void net_dev_async_completion(CALLBACK * pcb);
LOCAL void *net_dev_init_private(
				 struct link *,
				 int *,
				 long (*)(
					  struct link *,
					  struct sockaddr_in *,
					  int *,
					  void *
					  ),
				 void *
				 );
LOCAL void sync_io_completion(CALLBACK *);





/*******************************************************************************
 * Get io interrupt info
 *******************************************************************************/
long netDevGetIoIntInfo(int cmd, struct dbCommon * pxx, IOSCANPVT *ppvt)
{
  TRANSACTION *t;

  LOGMSG("devNetDev: netDevGetIoIntInfo(\"%s\",%d,0x%08x,0x%08x)\n",
	 pxx->name,cmd,pxx,ppvt,0,0,0,0,0);

  if (!pxx->dpvt) {
    errlogPrintf("devNetDev: netDevGetIoIntInfo is called with null dpvt\n");
    return ERROR;
  }

  t =  (TRANSACTION *) pxx->dpvt;

  if (t->io.event.ioscanpvt == NULL) {
    scanIoInit(&t->io.event.ioscanpvt);
  }

  *ppvt = t->io.event.ioscanpvt;

  return OK;
}





/*******************************************************************************
 * Xx Initialization (Called one time for each Xx record)
 *******************************************************************************/
long netDevInitXxRecord(
			struct dbCommon *pxx,
			struct link *plink,
			int option,
			void *device,
			long (*parse_link)(
					  struct link *,
					  struct sockaddr_in *,
					  int *,
					  void *
					  ),
			long (*config_command)(
					       dbCommon *,
					       int *,
					       uint8_t *,
					       int *,
					       void *,
					       int
					       ),
			long (*parse_response)(
					       dbCommon *,
					       int *,
					       uint8_t *,
					       int *,
					       void *,
					       int
					       )
			)
{
  TRANSACTION *t;

  LOGMSG("devNetDev: netDevInitXxRecord(\"%s\",0x%08x,%d,0x%08x,0x%08x,0x%08x)\n",
	 pxx->name,plink,option,device,parse_link,config_command,parse_response,0,0);

  if (!device || !parse_link ||
      (isNormal(option) && !config_command) ||
      (isEvent(option) && !parse_response))
    {
      pxx->pact = TRUE;
      errPrintf(ERROR, __FILE__, __LINE__,
		"Illeagal augument(\"%s\")\n", pxx->name);
      recGblSetSevr(pxx, INVALID_ALARM, INVALID_ALARM);
      return ERROR;
    }

  t = net_dev_init_private(
			   plink,
			   &option,
			   parse_link,
			   device
			   );

  if (!t)
    {
      pxx->pact = TRUE;
      errPrintf(ERROR, __FILE__, __LINE__,
		"Can't initialize device private(\"%s\")\n", pxx->name);
      recGblSetSevr(pxx, INVALID_ALARM, INVALID_ALARM);
      return ERROR;
    }

  t->record = pxx;
  t->option = option;
  t->device = (void *) device;
  t->config_command = config_command;
  t->parse_response = parse_response;

  if (isEvent(option))
    {
      if (drvNetMpfRegisterEvent(t))
	{
	  errPrintf(ERROR, __FILE__, __LINE__,
		    "Can't register event record (\"%s\")\n", pxx->name);
	  recGblSetSevr(pxx, INVALID_ALARM, INVALID_ALARM);
	  return ERROR;
	}
    }
  else
    {
      t->io.async.timeout = GET_TIMEOUT(option);
      if (isFineTmo(option))
	{
	  t->io.async.timeout /= TICK_PER_SECOND;
	}
      callbackSetCallback(net_dev_async_completion, &t->io.async.callback);
      callbackSetPriority(pxx->prio, &t->io.async.callback);
      callbackSetUser(pxx, &t->io.async.callback);
    }

  pxx->dpvt = (void *) t;

  return OK;
}





/*******************************************************************************
 * Perform read/write operation for Xx record
 *******************************************************************************/
long netDevReadWriteXx(struct dbCommon *pxx)
{
  TRANSACTION *t = (TRANSACTION *)pxx->dpvt;

  LOGMSG("devNetDev: netDevReadWriteXx(\"%s\",pact=%d, ret=%d)\n",
	 pxx->name,pxx->pact,t->ret,0,0,0,0,0,0);

  if (isEvent(t->option))
    {
      if (t->record->scan != SCAN_IO_EVENT)
	{
	  recGblSetSevr(pxx, INVALID_ALARM, INVALID_ALARM);
	  return ERROR;
	}

      if (t->ret)
	{
	  recGblSetSevr(pxx, isRead(t->option) ?
			READ_ALARM : WRITE_ALARM, INVALID_ALARM);
	}

      return t->ret;      
    }

  if (pxx->pact)
    {
      if (t->io.async.timeout_flag)
	{
	  recGblSetSevr(pxx, isRead(t->option) ?
			READ_ALARM : WRITE_ALARM, INVALID_ALARM);
	  return ERROR;
	}

      if (t->ret && t->ret != 2)
	{
	  recGblSetSevr(pxx, isRead(t->option) ?
			READ_ALARM : WRITE_ALARM, INVALID_ALARM);
	}

      return t->ret;
    }

  if (drvNetMpfSendRequest(t))
    {
      errPrintf(ERROR, __FILE__, __LINE__,
		"Record processing disabled(\"%s\")\n", pxx->name);
      recGblSetSevr(pxx, INVALID_ALARM, INVALID_ALARM);
      pxx->pact = TRUE;
      return ERROR;
    }

  pxx->pact = TRUE;

  return OK;
}





/*******************************************************************************
 * Asynchrouos read completion routine for Xx record
 *******************************************************************************/
LOCAL void net_dev_async_completion(CALLBACK *pcb)
{
  struct dbCommon *pxx;
  struct rset *rset;
  
  LOGMSG("devNetDev: net_dev_async_completion(0x%08x)\n",
	 pcb,0,0,0,0,0,0,0,0);

  callbackGetUser(pxx, pcb);
  rset = (struct rset *)(pxx->rset);
  dbScanLock(pxx);
  {
    (*rset->process)(pxx);
  }
  dbScanUnlock(pxx);
}





/*******************************************************************************
 * netDevInit()
 *******************************************************************************/
long netDevInit(void)
{
  if (init_flag != 0) return(OK);
  init_flag = 1;

  LOGMSG("devNetDev: netDevInit()\n",0,0,0,0,0,0,0,0,0);

  return OK;
}





/********************************************************************************
 * net_dev_init_private()
 ********************************************************************************/
LOCAL void *net_dev_init_private(
				 struct link *plink,
				 int *option,
				 long (*parse_link)(
						    struct link *,
						    struct sockaddr_in *,
						    int *,
						    void *
						    ),
				 void *device
				 )
{
  TRANSACTION *t;
  struct sockaddr_in peer_addr;

  LOGMSG("devNetDev: net_dev_init_private(0x%08x,%d,0x%08x,0x%08x)\n",
	 plink,option,parse_link,device,0,0,0,0,0);

  if (plink->type != INST_IO)
    {
      errlogPrintf("devNetDev: address type must be \"INST_IO\"\n");
      return NULL;
    }
 
  t = (TRANSACTION *) calloc(1, sizeof(TRANSACTION));

  if (!t)
    {
      errlogPrintf("devNetDev: no memory for TRANSACTION\n");
      return NULL;
    }

  memset(&peer_addr, 0, sizeof(peer_addr));

  if (parse_link(plink,
		 &peer_addr,
		 option,
		 device))
    {
      errlogPrintf ("devNetDev: syntax error in link field\n");
      return NULL;
    }

  if (isEvent(*option))
    {
      t->facility = drvNetMpfInitServer(
					ntohs(peer_addr.sin_port),
					*option
					);
      if (!t->facility)
	{
	  errlogPrintf ("devNetDev: drvNetMpfInitServer failed\n");
	  return NULL;
	}

      t->io.event.client_addr.sin_addr.s_addr = peer_addr.sin_addr.s_addr;
    }
  else
    {
      t->facility = drvNetMpfInitPeer(peer_addr, *option);

      if (!t->facility)
	{
	  errlogPrintf ("devNetDev: drvNetMpfInitPeer failed\n");
	  return NULL;
	}

      if (isOmron(*option))
	{
	  t->peer_addr = peer_addr;
	}
    }

  return t;
}






LOCAL void sync_io_completion(CALLBACK *pcb)
{
  epicsEventId  semId;

  LOGMSG("devNetDev: sync_io_completion(0x%08x)\n",
	 pcb,0,0,0,0,0,0,0,0);

  callbackGetUser(semId, pcb);
  epicsEventSignal(semId);
}



TRANSACTION *
netDevInitInternalIO(
		     struct dbCommon *pxx,
		     struct sockaddr_in peer_addr,
		     long (*config_command)(
					    struct dbCommon *,
					    int *,
					    uint8_t *,
					    int *,
					    void *,
					    int
					    ),
		     long (*parse_response)(
					    struct dbCommon *,
					    int *,
					    uint8_t *,
					    int *,
					    void *,
					    int
					    ),
		     void (*async_io_completion)(
						 CALLBACK *
						 ),
		     void *arg,
		     int protocol
		     )
{
  TRANSACTION *t;
  epicsEventId  semId;

  LOGMSG("devNetDev: netDevInitInternalIO(\"%s\")\n",
	 pxx->name,0,0,0,0,0,0,0,0);

  t = (TRANSACTION *) calloc(1, sizeof(TRANSACTION));
  if (!t)
    {
      errlogPrintf("devNetDev: no memory for TRANSACTION\n");
      return NULL;
    }

  t->record = pxx;
  t->config_command = config_command;
  t->parse_response = parse_response;

  if (async_io_completion)
    {
      callbackSetCallback(async_io_completion, &t->io.async.callback);
      callbackSetUser(arg, &t->io.async.callback);
    }
  else
    {
      if ((semId = 
	   epicsEventCreate(epicsEventEmpty)) == (epicsEventId) 0)
	{
	  errlogPrintf("devNetDev: semBCreate failed\n");
	  free((void *) t);
	  return NULL;
	}

      LOGMSG("devNetDev: binary semaphore 0x%08x created\n",
	     semId,0,0,0,0,0,0,0,0);

      callbackSetCallback(sync_io_completion, &t->io.async.callback);
      callbackSetUser(semId, &t->io.async.callback);
    }
  
  callbackSetPriority(priorityLow, &t->io.async.callback);

  t->facility = drvNetMpfInitPeer(peer_addr, protocol);
  if (!t->facility)
    {
      errlogPrintf ("devNetDev: drvNetMpfInitPeer failed\n");
      return NULL;
    }

  return t;
}




int netDevInternalIO(
		     int option,
		     TRANSACTION *t,
		     void *device,
		     int timeout
		     )
{
  TRANSACTION *t_org;
  epicsEventId  semId;

  LOGMSG("devNetDev: netDevInternalIO(%d,0x%08x,0x%08x,%d)\n",
	 option,t,device,timeout,0,0,0,0,0);

  if (!t || !t->record)
    {
      errlogPrintf("devNetDev: illeagal transaction sturucture\n");
      return ERROR;
    }

  t_org = (TRANSACTION *) t->record->dpvt;
  if (isNormal(t_org->option) && t->record->pact)
    {
      errlogPrintf("devNetDev: internal IO in async process\n");
      return ERROR;
    }

  t->option = option;
  t->device = device;
  t->io.async.timeout = timeout ? timeout : DEFAULT_TIMEOUT;

  if (drvNetMpfSendRequest(t))
    {
      errlogPrintf("devNetDev: drvNetMpfSendRequest failed\n");
      return ERROR;
    }

  if (t->io.async.callback.callback == sync_io_completion)
    {
      callbackGetUser(semId, &t->io.async.callback);

      LOGMSG("devNetDev: taking binary semaphore 0x%08x\n",
	     semId,0,0,0,0,0,0,0,0);

      epicsEventMustWait(semId);

      if (t->io.async.timeout_flag)
	{
	  return ERROR;
	}

      return t->ret;
    }

  return OK;
}




int netDevDeleteInternalIO(TRANSACTION *t)
{
  epicsEventId  semId;

  LOGMSG("devNetDev: netDevDeleteInternalIO(0x%08x)\n",
	 t,0,0,0,0,0,0,0,0);

  if (!t)
    {
      errlogPrintf("devNetDev: null transaction sturucture\n");
      return ERROR;
    }

  if (t->io.async.callback.callback == sync_io_completion)
    {
      callbackGetUser(semId, &t->io.async.callback);
      epicsEventDestroy(semId);

      LOGMSG("devNetDev: binary semaphore 0x%08x deleted\n",
	     semId,0,0,0,0,0,0,0,0);
    }

  free(t);

  return OK;
}





/*******************************************************************************
 * Get host(self) id in cpu native byte order
 *******************************************************************************/
#ifdef vxWorks
#include <sysLib.h>
/*
 * Modified from We7000_get_host_ip
 * Returns the IP address of IOC *** WITHOUT *** converting
 * it into network byte ordering.
 */
 uint32_t netDevGetSelfId(void)
{
  static long host_ip = 0;
  char temp[1024];
  char ead[1024];

  LOGMSG("devNetDev: netDevGetSelfId()\n",0,0,0,0,0,0,0,0,0);

  if (host_ip)
    return host_ip;

  if (bootStructToString(temp, &sysBootParams) != OK)
    {
      errlogPrintf("bootStructToString() error\n");
      return ERROR;
    }

  /* ead == `xxx.xxx.xxx.xxx:netmask' */
  (void) sscanf(sysBootParams.ead, "%[0-9.]", ead);

  host_ip = inet_addr(ead);
  if (host_ip == ERROR)
    {
      errlogPrintf("%s %s\n", sysBootParams.ead, ead);
      errlogPrintf("address lookup failure\n");
      return ERROR;
    }

  /* network byte order to host local byte order */
  return (uint32_t) ntohl(host_ip);
}
#else
#define MAX_HOST_NAME  (256)
uint32_t netDevGetSelfId(void)
{
  char hostname[MAX_HOST_NAME];
  int host_ip;

  LOGMSG("devNetDev: netDevGetSelfId()\n",0,0,0,0,0,0,0,0,0);

  if (gethostname(hostname, MAX_HOST_NAME) == ERROR)
    {
      errlogPrintf("gethostname() error\n");
      return ERROR;
    }

  if (netDevGetHostId(hostname, &host_ip) == ERROR)
    {
      errlogPrintf("netDevGetHostId() error\n");
      return ERROR;
    }

  /* network byte order to host local byte order */
  return (uint32_t) ntohl(host_ip);
}
#endif





/*******************************************************************************
 * Get host id
 *******************************************************************************/
#ifdef vxWorks
long netDevGetHostId(char *hostname, int *hostid)
{
  LOGMSG("devNetDev: netDevGetHostId(\"%s\",0x%08x)\n", hostname, hostid,
	 0,0,0,0,0,0,0);

  *hostid = hostGetByName(hostname);
  if (*hostid == ERROR)
    {
      *hostid = inet_addr(hostname);
      if (*hostid == ERROR)
	{
	  return ERROR;
	}
    }

  /* already in network byte order */
  return OK;
}
#else
long netDevGetHostId(char *hostname, in_addr_t *hostid)
{
  struct hostent *hostptr;

  LOGMSG("devNetDev: netDevGetHostId(\"%s\",0x%08x)\n", hostname, hostid,
	 0,0,0,0,0,0,0);

  if ((hostptr = gethostbyname(hostname)) != NULL)
    {
      *hostid = *((in_addr_t *) (hostptr->h_addr_list)[0]);
    }
  else
    {
      *hostid = inet_addr(hostname);
      if (*hostid == ERROR)
	{
	  return ERROR;
	}
    }

  /* already in network byte order */
  return OK;
}
#endif



/*******************************************************************************
 * Set event message length
 *******************************************************************************/
long netDevSetEvMsgLeng(struct dbCommon *pxx, int len)
{
  TRANSACTION *t = (TRANSACTION *)pxx->dpvt;

  LOGMSG("devNetDev: netDevSetEvMsgLeng(\"%s\",0x%08x)\n", pxx->name, len,
	 0,0,0,0,0,0,0);

  if (!t)
    {
      pxx->pact = TRUE;
      errPrintf(ERROR, __FILE__, __LINE__,
		"Record processing disabled(\"%s\")\n", pxx->name);
      recGblSetSevr(pxx, INVALID_ALARM, INVALID_ALARM);
      return ERROR;
    }

  t->io.event.ev_msg_len = len;

  return OK;
}



