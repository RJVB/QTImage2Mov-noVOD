// (C) 20100630 RJVB, INRETS
// SS_Log facility to C interface

#define _LOGGER_INTERNAL_
#include "Logger.h"

#include <stdarg.h>

static SS_Log qtLog;

static DWORD filter = LOGTYPE_LOGTOWINDOW|LOGTYPE_TRACE;

SS_Log *Initialise_Log(char *title, char *progName)
{
	qtLog.Filter( filter );
	qtLog.WindowName( title );
	if( progName && *progName ){
		qtLog.ProgName(progName);
	}
	qtLog.EraseLog();
	Log( &qtLog, "%s initalised", title );
	return &qtLog;
}

void cWriteLog(SS_Log *pLog, char* pMsg, ...)
{ va_list ap;
	va_start( ap, pMsg );
#ifdef _SS_LOG_ACTIVE
	WriteLog( &qtLog, filter, pMsg, &ap );
#endif
	va_end(ap);
}

void cLogStoreFileLine(char* szFile, int nLine)
{
#ifdef _SS_LOG_ACTIVE
	LogStoreFileLine( (TCHAR*) szFile, nLine );
#endif
}