/* devKeyPlc.c */
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

#include        <dbFldTypes.h>

#include        "drvNetMpf.h"
#include        "devNetDev.h"

#ifndef EPICS_REVISION
#include <epicsVersion.h>
#endif
#include <epicsExport.h>

#define KEY_DEFAULT_PORT 8501
#define KEY_GET_PROTO key_get_protocol()
#define KEY_MAX_NDATA 120
#define KEY_CMND_ST_RS  1
#define KEY_CMND_RD     2
#define KEY_CMND_RDE    3
#define KEY_CMND_WRE    4
#define KEY_CMND_LENGTH(x) ((x) == KEY_CMND_RDE || (x) == KEY_CMND_WRE) ? 18 : 12

LOCAL int sizeofTypes[] = {0,1,1,2,2,4,4,4,8,2};

LOCAL long key_parse_link(
			  struct link *,
			  struct sockaddr_in *,
			  int *,
			  void *
			  );
LOCAL int key_get_protocol(void);
LOCAL void *key_calloc(uint32_t, int);


typedef struct
{
  uint8_t  indx;
  uint32_t addr;
  int      cmnd;
  int      nleft;
  int      noff;
  char    *lopt;
} KEY_PLC;

typedef struct
{
  char *d_name;
  char *device;
  char *addr_format;
  char *suffix;
} CMND_INFO;

static CMND_INFO cmnd_tbl_ST_RS[] =
{
  {"R",   "",    "\%05d", NULL},
  {"MR",  "MR",  "\%05d", NULL},
  {"LR",  "LR",  "\%05d", NULL},
  {"CR",  "CR",  "\%04d", NULL},
  {"T",   "T",   "\%04d", NULL},
  {"C",   "C",   "\%04d", NULL},
  {"CTC", "CTC", "\%01d", NULL},
  {NULL, NULL, NULL, NULL}
};

static CMND_INFO cmnd_tbl_RD[] =
{
  {"R",   "",    "\%05d", NULL},
  {"MR",  "MR",  "\%05d", NULL},
  {"LR",  "LR",  "\%05d", NULL},
  {"CR",  "CR",  "\%04d", NULL},
  {NULL, NULL, NULL, NULL}
};

static CMND_INFO cmnd_tbl_RDE_WRE[] =
{
  {"R",  "",   "\%05d", ".U"},
  {"MR", "MR", "\%05d", ".U"},
  {"LR", "LR", "\%05d", ".U"},
  {"CR", "CR", "\%04d", ".U"},
  {"DM", "DM", "\%05d", ""},
  {"EM", "EM", "\%05d", ""},
  {"FM", "FM", "\%05d", ""},
  {"TM", "TM", "\%03d", ""},
  {"CM", "CM", "\%05d", ""},
  {NULL, NULL, NULL, NULL}
};

LOCAL long key_config_command(uint8_t *, int *, void *, int, int, int *, KEY_PLC *);
LOCAL long key_parse_response(uint8_t *, int *, void *, int, int, int *, KEY_PLC *);

int keyPlcUseTcp;


LOCAL int key_get_protocol(void)
{
  if (keyPlcUseTcp)
    {
      return MPF_TCP;
    }

  return MPF_UDP;
}


LOCAL void *key_calloc(
		       uint32_t addr,
		       int cmnd
		       )
{
  KEY_PLC *d;

  d = (KEY_PLC *) calloc(1, sizeof(KEY_PLC));
  if (!d)
    {
      errlogPrintf("devKeyPlc: calloc failed\n");
      return NULL;
    }

  d->cmnd = cmnd;
  d->addr = addr;

  return d;
}


#include        "devBiKeyPlc.c"
#include        "devBoKeyPlc.c"
#include        "devLonginKeyPlc.c"
#include        "devLongoutKeyPlc.c"
#include        "devAiKeyPlc.c"
#include        "devAoKeyPlc.c"
#include        "devMbbiDirectKeyPlc.c"
#include        "devMbboDirectKeyPlc.c"
#include        "devWaveformKeyPlc.c"
#include        "devArrayoutKeyPlc.c"



/*********************************************************************************
 * Link field parser for both command/response I/O and event driven I/O
 *********************************************************************************/
LOCAL long key_parse_link(
			  struct link *plink,
			  struct sockaddr_in *peer_addr,
			  int *option,
			  void *device
			  )
{
  char *protocol = NULL;
  char *route = NULL;
  char *unit  = NULL;
  char *type  = NULL;
  char *addr  = NULL;
  char *lopt  = NULL;
  KEY_PLC *d = (KEY_PLC *) device;
  CMND_INFO *pcmnd;
  int i;
  int found = 0;

  if (parseLinkPlcCommon(
			 plink,
			 peer_addr,
			 &protocol,
			 &route, /* dummy */
			 &unit,
			 &type,
			 &addr,
			 &lopt
			 ))
    {
      errlogPrintf("devKeyPlc: illeagal input specification\n");
      return ERROR;
    }

  if (!peer_addr->sin_port)
    {
      peer_addr->sin_port = htons(KEY_DEFAULT_PORT);
      LOGMSG("devKeyPlc: port: 0x%04x\n",ntohs(peer_addr->sin_port),
	     0,0,0,0,0,0,0,0);
    }

  if (unit)
    {
      errlogPrintf("devKeyPlc: neglected specified unit \"%s\"\n", unit);
      /* keep going */
    }

  if (!type)
    {
      errlogPrintf("devKeyPlc: no device type specified\n");
      return ERROR;
    }

  switch (d->cmnd)
    {
    case KEY_CMND_RD:
      {
	pcmnd = &cmnd_tbl_RD[0];
      }
      break;
    case KEY_CMND_ST_RS:
      {
	pcmnd = &cmnd_tbl_ST_RS[0];
      }
      break;
    case KEY_CMND_RDE:
    case KEY_CMND_WRE:
      {
	pcmnd = &cmnd_tbl_RDE_WRE[0];
      }
      break;
    default:
      errlogPrintf("devKeyPlc: illeagal data cmnd %d\n", d->cmnd);
      return ERROR;
    }

  for (i = 0; pcmnd[i].d_name != NULL; i++)
    {
      if (strncmp(pcmnd[i].d_name, type, 2) == 0)
	{
	  found = 1;
	  d->indx = i;
	}
    }

  if (!found)
    {
      errlogPrintf("devKeyPlc: unrecognized device (\"%s\")\n", type);
      return ERROR;
    }

  if (!addr)
    {
      errlogPrintf("devKeyPlc: no address specified\n");
      return ERROR;
    }

  if (strncmp(addr, "0x", 2) == 0)
    {
      if (sscanf(addr, "%x", &d->addr) != 1)
	{
	  errlogPrintf("devKeyPlc: can't get device addr\n");
	  return ERROR;
	}
    }
  else
    {
      if (sscanf(addr, "%d", &d->addr) != 1)
	{
	  errlogPrintf("devKeyPlc: can't get device addr\n");
	  return ERROR;
	}
    }

  return OK;
}





/******************************************************************************
 * Command constructor for command/response I/O
 ******************************************************************************/
LOCAL long key_config_command(
			      uint8_t *buf,    /* driver buf addr     */
			      int     *len,    /* driver buf size     */
			      void    *bptr,   /* record buf addr     */
			      int      ftvl,   /* record field type   */
			      int      ndata,  /* n to be transferred */
			      int     *option, /* direction etc.      */
			      KEY_PLC *d
			      )
{
  static char format[256];
  int nwrite;
  int n;

  LOGMSG("devKeyPlc: key_config_command(0x%08x,%d,0x%08x,%d,%d,%d,0x%08x)\n",
	 buf,*len,bptr,ftvl,ndata,*option,d,0,0);

  if (ndata > KEY_MAX_NDATA)
    {
      if (!d->noff)
	{
	  /* for the first time */
	  d->nleft = ndata;
	}

      n = (d->nleft > KEY_MAX_NDATA) ? KEY_MAX_NDATA : d->nleft;
    }
  else
    {
      n = ndata;
    }

  nwrite = (d->cmnd == KEY_CMND_WRE) ? 6*n : 0;

  if (*len < (KEY_CMND_LENGTH(d->cmnd) + nwrite + 1))
    {
      errlogPrintf("devKeyPlc: buffer is running short (max:%d, len:%d)\n",
		   *len, KEY_CMND_LENGTH(d->cmnd) + nwrite + 1);
      return ERROR;
    }

  switch (d->cmnd)
    {
    case KEY_CMND_ST_RS:
      {
	if (*((unsigned long *) bptr))
	  {
	    sprintf(format, "ST %s%s",
		    cmnd_tbl_RDE_WRE[d->indx].device,
		    cmnd_tbl_RDE_WRE[d->indx].addr_format);
	  }
	else
	  {
	    sprintf(format, "RS %s%s",
		    cmnd_tbl_RDE_WRE[d->indx].device,
		    cmnd_tbl_RDE_WRE[d->indx].addr_format);
	  }
	sprintf(buf, format, d->addr);
      }
      break;
    case KEY_CMND_RD:
      {
	sprintf(format, "RD %s%s",
		cmnd_tbl_RDE_WRE[d->indx].device,
		cmnd_tbl_RDE_WRE[d->indx].addr_format);
	sprintf(buf, format, d->addr);
      }
      break;
    case KEY_CMND_RDE:
      {
	sprintf(format, "RDE %s%s%s %03d",
		cmnd_tbl_RDE_WRE[d->indx].device,
		cmnd_tbl_RDE_WRE[d->indx].addr_format,
		cmnd_tbl_RDE_WRE[d->indx].suffix,
		n);
	sprintf(buf, format, d->addr + d->noff);
      }
      break;
    case KEY_CMND_WRE:
      {
	static uint16_t temp_buf[KEY_MAX_NDATA];
	unsigned int data;
	int i, j;

	sprintf(format, "WRE %s%s%s %03d",
		cmnd_tbl_RDE_WRE[d->indx].device,
		cmnd_tbl_RDE_WRE[d->indx].addr_format,
		cmnd_tbl_RDE_WRE[d->indx].suffix,
		n);
	sprintf(buf, format, d->addr);

	if (fromRecordVal(
			  temp_buf,
			  2,
			  bptr,
			  d->noff,
			  ftvl,
			  n,
			  0
			  ))
	  {
	    errlogPrintf("devKeyPlc: illeagal conversion\n");
	    return ERROR;
	  }

	j = (int) strlen(buf);

	for (i = 0; i < n; i++)
	  {
	    data = (unsigned int) temp_buf[i];
	    sprintf(&buf[j + 6*i], " %05d", (int) data);
	  }
      }
      break;
    default:
      {
	errlogPrintf("devKeyPlc: unsupported command\n");
	return ERROR;
      }
    }

  strcat(buf, "\r");
  *len = (int) strlen(buf);

  return OK;
}




/*******************************************************************************
 * Response parser for command/response I/O
 *******************************************************************************/
LOCAL long key_parse_response(
			      uint8_t *buf,    /* driver buf addr     */
			      int     *len,    /* driver buf size     */
			      void    *bptr,   /* record buf addr     */
			      int      ftvl,   /* record field type   */
			      int      ndata,  /* n to be transferred */
			      int     *option, /* direction etc.      */
			      KEY_PLC *d
			      )
{
  char c1, c2;
  long ret;
  int n;

  LOGMSG("devKeyPlc: key_parse_response(0x%08x,%d,0x%08x,%d,%d,%d,0x%08x)\n",
	 buf,len,bptr,ftvl,ndata,*option,d,0,0);

  if (ndata > KEY_MAX_NDATA)
    {
      n   = (d->nleft > KEY_MAX_NDATA) ? KEY_MAX_NDATA : d->nleft;
      ret = (d->nleft > KEY_MAX_NDATA) ? NOT_DONE : 0;
    }
  else
    {
      n = ndata;
      ret = 0;
    }

  if (sscanf(&buf[0], "%c%c\r\n", &c1, &c2) == 2)
    {
      if (c1 == 'E')
	{
	  switch (c2)
	    {
	    case '0':
	      errlogPrintf("devKeyPlc: device number (%d) out of range\n", d->addr);
	      return ERROR;
	    case '1':
	      errlogPrintf("devKeyPlc: unsupported command (%d)\n", d->cmnd);
	      return ERROR;
	    default:
	      errlogPrintf("devKeyPlc: unrecognized error code\n");
	      return ERROR;
	    }
	}
      else if ((d->cmnd == KEY_CMND_ST_RS  || d->cmnd == KEY_CMND_WRE ) &&
	       (c1 == 'O' && c2 == 'K'))
	{
	  return ret;
	}
      else if (d->cmnd == KEY_CMND_RD)
	{
	  if (sscanf(&buf[0], "%c\r\n", &c1) != 1)
	    {
	      errlogPrintf("devKeyPlc: failed in getting bit data\n");
	      return ERROR;
	    }

	  switch (c1)
	    {
	    case '0':
	      *((unsigned long *) bptr) = 0;
	      return ret;
	    case '1':
	      *((unsigned long *) bptr) = 1;
	      return ret;
	    default:
	      errlogPrintf("devKeyPlc: unrecognized error code\n");
	      return ERROR;
	    }
	}
      else if (d->cmnd == KEY_CMND_RDE)
	{
	  static int16_t temp_buf[KEY_MAX_NDATA];
	  int data;
	  int i;

	  for (i = 0; i < n; i++)
	    {
	      if (sscanf(&buf[6*i], "%05d", &data) != 1)
		{
		  errlogPrintf("devKeyPlc: failed in getting data\n");
		  return ERROR;
		}
	      temp_buf[i] = (int16_t) data;
	      if (i != n-1)
		{
		  if (buf[6*i + 5] != ' ')
		    {
		      errlogPrintf("devKeyPlc: unrecognized delimiter\n");
		      return ERROR;
		    }
		}
	      else
		{
		  if (strncmp(&buf[6*i + 5], "\r\n", 2))
		    {
		      errlogPrintf("devKeyPlc: illeagal terminator\n");
		      return ERROR;
		    }
		}
	    }

	  if (toRecordVal(
			  bptr,
			  d->noff,
			  ftvl,
			  temp_buf,
			  2,
			  n,
			  0
			  ))
	    {
	      errlogPrintf("devKeyPlc: illeagal or error response\n");
	      return ERROR;
	    }
	}
      else
	{
	  errlogPrintf("devKeyPlc: unrecognized response\n");
	  return ERROR;
	}
    }

  if (ndata > KEY_MAX_NDATA)
    {
      d->nleft -= n;
      d->noff  += n;

      if (!d->nleft)
	{
	  /* for the last time */
	  d->noff = 0;
	}
    }

  return ret;
}



