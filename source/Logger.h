// (C) 20100630 RJVB, INRETS

#ifndef _LOGGER_H_

#ifdef _LOGGER_INTERNAL_
#	include "SS_Log_Include.h"
#endif

#ifdef __cplusplus
	extern "C" {
#endif

#ifndef _LOGGER_INTERNAL_
	typedef void	SS_Log;
#	ifdef _SS_LOG_ACTIVE

#	define Log cLogStoreFileLine(__FILE__, __LINE__), cWriteLog


#else // _SS_LOG_ACTIVE

#	define Log

#endif // _SS_LOG_ACTIVE

#endif

extern SS_Log *Initialise_Log(char *title, char *progName);
extern void cWriteLog(SS_Log *pLog, char* pMsg, ...);
extern void cLogStoreFileLine(char* szFile, int nLine);


#ifdef __cplusplus
}
#endif

#define _LOGGER_H_
#endif