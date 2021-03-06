/*
 *  Logging.h
 *  QTImage2Mov
 *
 *  Created by Ren� J.V. Bertin on 20100713.
 *  Copyright 2010 INRETS / RJVB. All rights reserved.
 *
 */

#ifndef _LOGGING_H

#include <stdio.h>

#if defined(_WINDOWS) || defined(WIN32) || defined(_MSC_VER) || TARGET_OS_WIN32
#	include "Logger.h"
#	ifdef __cplusplus
extern "C" {
#	endif
#	ifdef HAS_LOG_INIT
		SS_Log *qtLogPtr = NULL;
		int qtLog_Initialised = 0;
#	else
		extern SS_Log *qtLogPtr;
		extern int qtLog_Initialised;
#	endif
#	ifdef __cplusplus
}
#	endif
#else
#	include <libgen.h>
#	ifdef _PC_LOG_ACTIVE
#		ifndef _PC_LOG_EMBEDDED
#			include <LogController/PCLogController.h>
#		else
#			include "PCLogController.h"
#		endif
//#		define	Log		fprintf
//#		define	qtLogPtr	stderr
		extern int PCLogStatusWithCFormat( id, const char*, int, char*, ... );
		extern BOOL PCLogActive();
		extern void *qtLogName();
//		extern void *nsString(char *s);
#		define	Log(d,f,...)	PCLogStatusWithCFormat((d),basename(__FILE__),__LINE__,((char*)(f)), ##__VA_ARGS__)
#		define	qtLogPtr		qtLogName()
#	else
#		include "NSLoggerClient.h"
#		define	Log(d,f,...)	NSLogprintf(basename(__FILE__),__LINE__,__FUNCTION__,1,\
								(f), ##__VA_ARGS__)
#		define	qtLogPtr		/**/
#	endif
#endif


#define _LOGGING_H
#endif
