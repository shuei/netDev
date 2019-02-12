/* devYewPlc.c */
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
 * 2006/01/12 jio
 *  added support to access registers of "Special Modules"
 *  enabled to select TCP/UDP dinamically
 * 2006/02/22 jio: added support for long word and floating point data,
 * made all integer data signed
 */

#include        <ctype.h>
#include        <dbFldTypes.h>

#include        "drvNetMpf.h"
#include        "devNetDev.h"

#ifndef EPICS_REVISION
#include <epicsVersion.h>
#endif
#if EPICS_REVISION == 14 && EPICS_MODIFICATION >= 2
#include <epicsExport.h>
#endif

#define YEW_CMND_LENGTH(x) ((x) ? 10 : 12)
#define YEW_DATA_OFFSET  4
#define YEW_DEFAULT_UNIT 0x01
#define YEW_DEFAULT_PORT 0x3001
#define YEW_GET_PROTO yew_get_protocol()
#define YEW_MAX_NDATA yewGetMaxTransfer()
#define YEW_NEEDS_SWAP (0x00ff != htons(0x00ff))
#define YEW_SPECIAL_MODULE

LOCAL int sizeofTypes[] = {0,1,1,2,2,4,4,4,8,2};
LOCAL int yew_max_ndata = 64;

LOCAL long yew_parse_link(
			  struct link *,
			  struct sockaddr_in *,
			  int *,
			  void *
			  );
LOCAL int yew_get_protocol(void);
LOCAL void *yew_calloc(int, uint8_t, uint32_t, int);


typedef struct
{
  int      unit;
  uint8_t  type;
  uint32_t addr;
  int      width;
  int      nleft;
  int      noff;
  int      dword;
  int      fpdat;
  int      spmod;
  int      m_unit;
  int      m_slot;
  char    *lopt;
} YEW_PLC;

LOCAL long yew_config_command(uint8_t *, int *, void *, int, int, int *, YEW_PLC *);
LOCAL long yew_parse_response(uint8_t *, int *, void *, int, int, int *, YEW_PLC *);

LOCAL long yew_parse_event(uint8_t *, int *, void *, int, int, int *, YEW_PLC *);
LOCAL long yew_config_response(uint8_t *, int *, void *, int, int, int *, YEW_PLC *);


int yewPlcUseTcp;

int yewGetMaxTransfer(void);
void yewSetMaxTransfer(int);


LOCAL int yew_get_protocol(void)
{
  if (yewPlcUseTcp)
    {
      return MPF_TCP;
    }

  return MPF_UDP;
}


int yewGetMaxTransfer(void)
{
  return yew_max_ndata;
}


void yewSetMaxTransfer(int ndata)
{
  yew_max_ndata = ndata;
}


LOCAL void *yew_calloc(
		       int unit,
		       uint8_t type,
		       uint32_t addr,
		       int width
		       )
{
  YEW_PLC *d;

  d = (YEW_PLC *) calloc(1, sizeof(YEW_PLC));
  if (!d)
    {
      errlogPrintf("devYewPlc: calloc failed\n");
      return NULL;
    }
      
  d->unit  = unit;
  d->type  = type;
  d->addr  = addr;
  d->width = width;

  return d;
}




#include        "devAiYewPlc.c"
#include        "devAoYewPlc.c"
#include        "devBiYewPlc.c"
#include        "devBoYewPlc.c"
#include        "devLonginYewPlc.c"
#include        "devLongoutYewPlc.c"
#include        "devMbbiDirectYewPlc.c"
#include        "devMbboDirectYewPlc.c"
#include        "devWaveformYewPlc.c"
#include        "devArrayoutYewPlc.c"
#include        "devPatternYewPlc.c"
#include        "devStatusYewPlc.c"





/*********************************************************************************
 * Link field parser for both command/response I/O and event driven I/O
 *********************************************************************************/
LOCAL long yew_parse_link(
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
  YEW_PLC *d  = (YEW_PLC *) device;

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
      errlogPrintf("devYewPlc: illeagal input specification\n");
      return ERROR;
    }

  if (!peer_addr->sin_port)
    {
      peer_addr->sin_port = htons(YEW_DEFAULT_PORT);
      LOGMSG("devYewPlc: port: 0x%04x\n",ntohs(peer_addr->sin_port),
	     0,0,0,0,0,0,0,0);
    }

  if (protocol)
    {
      if (strncmp(protocol, "TCP", 3) == 0 ||
	  strncmp(protocol, "tcp", 3) == 0)
	{
	  setTcp(*option);
	}
      else if (strncmp(protocol, "UDP", 3) == 0 ||
	       strncmp(protocol, "udp", 3) == 0)
	{
	  setUdp(*option);
	}
      else
	{
	  errlogPrintf("devYewPlc: unrecognized protocol\n");
	  return ERROR;
	}
    }

  if (unit)
    {
      if (strncmp(unit, "0x", 2) == 0)
	{
	  if (sscanf(unit, "%x", &d->unit) != 1)
	    {
	      errlogPrintf("devYewPlc: can't get unit\n");
	      return ERROR;
	    }
	}
      else
	{
	  if (sscanf(unit, "%d", &d->unit) != 1)
	    {
	      errlogPrintf("devYewPlc: can't get unit\n");
	      return ERROR;
	    }
	}
      LOGMSG("devYewPlc: unit: %d\n",d->unit,0,0,0,0,0,0,0,0);
    }
  else
    {
      d->unit = YEW_DEFAULT_UNIT;
      LOGMSG("devYewPlc: unit: %d\n",d->unit,0,0,0,0,0,0,0,0);
    }

  if (!addr)
    {
      errlogPrintf("devYewPlc: no address specified\n");
      return ERROR;
    }

  if (type)
    {
      if (isalpha(type[0]))
	{
	  /* CPU Module */

	  if (sscanf(type, "%c", &d->type) != 1)
	    {
	      errlogPrintf("devYewPlc: can't get device type\n");
	      return ERROR;
	    }
	}
#ifdef YEW_SPECIAL_MODULE
      else
	{
	  /* Special Module, "type" holds module unit/slot number */

	  if (sscanf(type, "%d,%d", &d->m_unit, &d->m_slot) != 2)
	    {
	      errlogPrintf("devYewPlc: can't get module unit and/or slot number\n");
	      return ERROR;
	    }

	  d->spmod = 1;
	}
#endif
      if (strncmp(addr, "0x", 2) == 0)
	{
	  if (sscanf(addr, "%x", &d->addr) != 1)
	    {
	      errlogPrintf("devYewPlc: can't get device addr\n");
	      return ERROR;
	    }
	}
      else
	{
	  if (sscanf(addr, "%d", &d->addr) != 1)
	    {
	      errlogPrintf("devYewPlc: can't get device addr\n");
	      return ERROR;
	    }
	}
    }
  else
    {
      /*
       * for backward compatibility
       * no delimiter (:) between "type" and "addr"
       */

      if (strncmp(addr + 1, "0x", 2) == 0)
	{
	  if (sscanf(addr, "%c%x", &d->type, &d->addr) != 2)
	    {
	      errlogPrintf("devYewPlc: can't get device type or addr\n");
	      return ERROR;
	    }
	}
      else
	{
	  if (sscanf(addr, "%c%d", &d->type, &d->addr) != 2)
	    {
	      errlogPrintf("devYewPlc: can't get device type or addr\n");
	      return ERROR;
	    }
	}
    }

  if (lopt)
    {
      if (lopt[0] == 'L')
	{
	  d->dword = 1;
	  LOGMSG("devYewPlc: found option to handle the data as long word data\n",0,0,0,0,0,0,0,0,0);
	}
      else if (lopt[0] == 'F')
	{
	  d->fpdat = 1;
	  LOGMSG("devYewPlc: found option to handle the data as floating point data\n",
		 0,0,0,0,0,0,0,0,0);
	}

      d->lopt = lopt;
    }

  if (!d->spmod)
    {
      /* CPU Module */

      switch (d->width)
	{
	case 1:
	  {
	    switch (d->type)
	      {
	      case  'X':    /* input relay         */
		{
		  if (isWrite(*option))
		    {
		      errlogPrintf("devYewPlc: write access to read-only device\n");
		      return ERROR;
		    }
		}
	      case  'Y':    /* output relay        */
	      case  'I':    /* internal relay      */
	      case  'E':    /* shared relay        */
	      case  'M':    /* special relay       */
	      case  'T':    /* timer relay         */
	      case  'C':    /* counter relay       */
	      case  'L':    /* link relay          */
		break;
	      default:
		errlogPrintf("devYewPlc: illeagal device specification \'%c\' (width %d)\n",
			     d->type, d->width);
		return ERROR;
	      }
	  }
	  break;
	case 2:
	  {
	    switch (d->type)
	      {
	      case  'X':    /* input relay         */
	      case 0x20:    /* timer set           */
	      case 0x30:    /* counter set         */
		{
		  if (isWrite(*option))
		    {
		      errlogPrintf("devYewPlc: write access to read-only device\n");
		      return ERROR;
		    }
		}
	      case  'Y':    /* output relay        */
	      case  'I':    /* internal relay      */
	      case  'E':    /* shared relay        */
	      case  'M':    /* special relay       */
	      case  'T':    /* timer relay         */
	      case  'C':    /* counter relay       */
	      case  'L':    /* link relay          */
	      case  'D':    /* data register       */
	      case  'B':    /* file register       */
	      case  'R':    /* shared register     */
	      case  'V':    /* index regigister    */
	      case  'Z':    /* special regigister  */
	      case  'W':    /* link regigister     */
	      case 0x61:    /* timer current       */
	      case 0x65:    /* timer current (count up) */
	      case 0x70:    /* counter set         */
	      case 0x71:    /* counter current     */
	      case 0x75:    /* counter current (count up) */
		break;
	      default:
		errlogPrintf("devYewPlc: unknown device type \'%c\'\n", d->type);
		return ERROR;
	      }
	  }
	  break;
	default:
	  errlogPrintf("devYewPlc: illeagal data width %d\n", d->width);
	  return ERROR;
	}

      d->type = d->type - '@'; /* subtract 0x40 to get data class */
    }
  else
    {
      /* Special Module */

      if (d->m_slot < 1 || d->m_slot > 16)
	{
	  errlogPrintf("devYewPlc: slot number (%d) out of range\n", d->m_slot);
	  return ERROR;
	}
    }

  return OK;
}





/******************************************************************************
 * Command constructor for command/response I/O
 ******************************************************************************/
LOCAL long yew_config_command(
			      uint8_t *buf,    /* driver buf addr     */
			      int     *len,    /* driver buf size     */
			      void    *bptr,   /* record buf addr     */
			      int      ftvl,   /* record field type   */
			      int      ndata,  /* n to be transferred */
			      int     *option, /* direction etc.      */
			      YEW_PLC *d
			      )
{
  int nwrite;
  int n;
  uint8_t  command_type;
  uint16_t bytes_follow;

  LOGMSG("devYewPlc: yew_config_command(0x%08x,%d,0x%08x,%d,%d,%d,0x%08x)\n",
	 buf,*len,bptr,ftvl,ndata,*option,d,0,0);

  if (ndata > YEW_MAX_NDATA)
    {
      if (!d->noff)
	{
	  /* for the first time */
	  d->nleft = ndata;
	}

      n = (d->nleft > YEW_MAX_NDATA) ? YEW_MAX_NDATA : d->nleft;
    }
  else
    {
      n = ndata;
    }

  nwrite = isWrite(*option) ? (d->width)*n : 0;

  if (*len < YEW_CMND_LENGTH(d->spmod) + nwrite)
    {
      errlogPrintf("devYewPlc: buffer is running short\n");
      return ERROR;
    }

  if (!d->spmod)
    {
      /* CPU Module */

      switch (d->width)
	{
	case 1:  /* bit device */
	  {
	    command_type = isRead(*option)? 0x01:0x02;
	    bytes_follow = isRead(*option)? htons(0x0008):htons(0x0008 + n);
	  }
	  break;
	case 2:  /* word device */
	  {
	    command_type = isRead(*option)? 0x11:0x12;
	    bytes_follow = isRead(*option)? htons(0x0008):htons(0x0008 + n*2);
	  }
	  break;
	default:
	  {
	    errlogPrintf("devYewPlc: unsupported data width\n");
	    return ERROR;
	  }
	}

      *((uint8_t  *) &buf[ 0]) = command_type;             /* R/W by bit/word       */
      *((uint8_t  *) &buf[ 1]) = d->unit;                  /* CPU No.               */
      *((uint16_t *) &buf[ 2]) = bytes_follow;             /* n of data below       */
      *((uint16_t *) &buf[ 4]) = htons(d->type);           /* device type           */
      *((uint32_t *) &buf[ 6]) = htonl(d->addr + d->noff); /* device addr           */
      *((uint16_t *) &buf[10]) = htons(n);                 /* n of data             */
    }
  else
    {
      /* Special Module, "type" holds module unit/slot number */

      command_type = isRead(*option)? 0x31:0x32;
      bytes_follow = isRead(*option)? htons(0x0006):htons(0x0006 + n*2);

      *((uint8_t  *) &buf[ 0]) = command_type;             /* R/W by bit/word       */
      *((uint8_t  *) &buf[ 1]) = d->unit;                  /* CPU No.               */
      *((uint16_t *) &buf[ 2]) = bytes_follow;             /* n of data below       */
      *((uint8_t  *) &buf[ 4]) = d->m_unit;                /* module unit           */
      *((uint8_t  *) &buf[ 5]) = d->m_slot;                /* module slot           */
      *((uint16_t *) &buf[ 6]) = htons(d->addr + d->noff); /* data position         */
      *((uint16_t *) &buf[ 8]) = htons(n);                 /* n of data             */
    }

  if (isWrite(*option))
    {
      if (fromRecordVal(
			&buf[YEW_CMND_LENGTH(d->spmod)],
			d->width,
			bptr,
			d->noff,
			ftvl,
			n,
			YEW_NEEDS_SWAP
			))
	{
	  errlogPrintf("devYewPlc: illeagal command\n");
	  return ERROR;
	}
    }

  *len = YEW_CMND_LENGTH(d->spmod) + nwrite;

  /*
  nread  = isRead(*option)? (d->width)*n:0;
  return (YEW_DATA_OFFSET + nread);
  */
  return 0;
}




/*******************************************************************************
 * Response parser for command/response I/O
 *******************************************************************************/
LOCAL long yew_parse_response(
			      uint8_t *buf,    /* driver buf addr     */
			      int     *len,    /* driver buf size     */
			      void    *bptr,   /* record buf addr     */
			      int      ftvl,   /* record field type   */
			      int      ndata,  /* n to be transferred */
			      int     *option, /* direction etc.      */
			      YEW_PLC *d
			      )
{
  int nread;
  int n;
  int ret;
  uint8_t  response_type;
  uint16_t number_of_data;

  LOGMSG("devYewPlc: yew_parse_response(0x%08x,%d,0x%08x,%d,%d,%d,0x%08x)\n",
	 buf,len,bptr,ftvl,ndata,*option,d,0,0);

  if (ndata > YEW_MAX_NDATA)
    {
      n   = (d->nleft > YEW_MAX_NDATA) ? YEW_MAX_NDATA : d->nleft;
      ret = (d->nleft > YEW_MAX_NDATA) ? NOT_DONE : 0;
    }
  else
    {
      n = ndata;
      ret = 0;
    }

  nread  = isRead(*option)? (d->width)*n:0; 

  if (!d->spmod)
    {
      /* CPU Module */

      switch (d->width)
	{
	case 1:  /* bit device */
	  {
	    response_type  = isRead(*option)? 0x81:0x82;
	    number_of_data = isRead(*option)? htons(n):htons(0x0000);
	  }
	  break;
	case 2:  /* word device */
	  {
	    response_type  = isRead(*option)? 0x91:0x92;
	    number_of_data = isRead(*option)? htons(n*2):htons(0x0000);
	  }
	  break;
	default:
	  {
	    errlogPrintf("devYewPlc: unsupported data width\n");
	    return ERROR;
	  }
	}
    }
  else
    {
      /* Special Module */

      response_type  = isRead(*option)? 0xB1:0xB2;
      number_of_data = isRead(*option)? htons(n*2):htons(0x0000);
    }

  if (buf[0] != response_type)
    {
      errlogPrintf("devYewPlc: non-response returned (0x%02X)\n", buf[0]);
      return NOT_MINE;
    }

  if (buf[1] != 0x00)
    {
      errlogPrintf("devYewPlc: error termination code returned (0x%02X)\n",
		   buf[1]);
      return ERROR;
    }

  if (*((uint16_t *) &buf[2]) != number_of_data)
    {
      errlogPrintf("devYewPlc: number of data does not match\n");
      return NOT_MINE;
    }

  if (isRead(*option))
    {
      if (toRecordVal(
		      bptr,
		      d->noff,
		      ftvl,
		      &buf[YEW_DATA_OFFSET],
		      d->width,
		      n,
		      YEW_NEEDS_SWAP
		      ))
	{
	  errlogPrintf("devYewPlc: illeagal or error response\n");
	  return ERROR;
	}
    }

  if (ndata > YEW_MAX_NDATA)
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



